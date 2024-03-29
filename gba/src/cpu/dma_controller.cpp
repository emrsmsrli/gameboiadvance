/*
 * Copyright (C) 2020  emrsmsrli
 * 
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#include <gba/cpu/dma_controller.h>

#include <algorithm>

#include <gba/archive.h>
#include <gba/cpu/bus_interface.h>
#include <gba/helper/range.h>

namespace gba::dma {

namespace {

constexpr array channel_masks{
  data{0x07FF'FFFF_u32, 0x07FF'FFFF_u32, 0x3FFF_u32},
  data{0x0FFF'FFFF_u32, 0x07FF'FFFF_u32, 0x3FFF_u32},
  data{0x0FFF'FFFF_u32, 0x07FF'FFFF_u32, 0x3FFF_u32},
  data{0x0FFF'FFFF_u32, 0x0FFF'FFFF_u32, 0xFFFF_u32}
};

void sort_by_priority(static_vector<channel*, channel_count>& channels) noexcept
{
    std::sort(channels.begin(), channels.end(), [](const channel* l, const channel* r) {
        return l->id > r->id;
    });
}

[[nodiscard]] FORCEINLINE bool is_for_fifo(channel* channel) noexcept
{
    return channel->cnt.when == channel::control::timing::special &&
      (channel->id == 1_u32 || channel->id == 2_u32);
}

[[nodiscard]] FORCEINLINE bool addr_in_rom_area(const u32 addr) noexcept
{
    const u32 page = addr >> 24_u32;
    return 0x08_u32 <= page && page <= 0x0D_u32;
}

} // namespace

void channel::write_dst(const u8 n, const u8 data) noexcept
{
    dst = bit::set_byte(dst, n, data) & channel_masks[id].dst;
}

void channel::write_src(const u8 n, const u8 data) noexcept
{
    src = bit::set_byte(src, n, data) & channel_masks[id].src;
}

void channel::write_count(const u8 n, const u8 data) noexcept
{
    count = bit::set_byte(count, n, data) & channel_masks[id].count;
    if(count == 0_u32) {
        count = channel_masks[id].count + 1_u32;
    }
}

u8 channel::read_cnt_l() const noexcept
{
    return from_enum<u8>(cnt.dst_control) << 5_u8
      | bit::extract(from_enum<u8>(cnt.src_control), 0_u8) << 7_u8;
}

u8 channel::read_cnt_h() const noexcept
{
    return bit::from_bool<u8>(cnt.enabled) << 7_u8
      | bit::from_bool<u8>(cnt.irq) << 6_u8
      | from_enum<u8>(cnt.when) << 4_u8
      | bit::from_bool<u8>(cnt.drq) << 3_u8
      | from_enum<u8>(cnt.size) << 2_u8
      | bit::from_bool<u8>(cnt.repeat) << 1_u8
      | bit::extract(from_enum<u8>(cnt.src_control), 1_u8);
}

void channel::serialize(archive& archive) const noexcept
{
    archive.serialize(last_event_handle);
    archive.serialize(src);
    archive.serialize(dst);
    archive.serialize(count);
    archive.serialize(internal.src);
    archive.serialize(internal.dst);
    archive.serialize(internal.count);
    archive.serialize(cnt.dst_control);
    archive.serialize(cnt.src_control);
    archive.serialize(cnt.when);
    archive.serialize(cnt.repeat);
    archive.serialize(cnt.size);
    archive.serialize(cnt.drq);
    archive.serialize(cnt.irq);
    archive.serialize(cnt.enabled);
    archive.serialize(latch);
}

void channel::deserialize(const archive& archive) noexcept
{
    archive.deserialize(last_event_handle);
    archive.deserialize(src);
    archive.deserialize(dst);
    archive.deserialize(count);
    archive.deserialize(internal.src);
    archive.deserialize(internal.dst);
    archive.deserialize(internal.count);
    archive.deserialize(cnt.dst_control);
    archive.deserialize(cnt.src_control);
    archive.deserialize(cnt.when);
    archive.deserialize(cnt.repeat);
    archive.deserialize(cnt.size);
    archive.deserialize(cnt.drq);
    archive.deserialize(cnt.irq);
    archive.deserialize(cnt.enabled);
    archive.deserialize(latch);
}

controller::controller(cpu::bus_interface* bus, cpu::irq_controller_handle irq, scheduler* scheduler) noexcept
  : bus_{bus}, irq_{irq}, scheduler_{scheduler}
{
    hw_event_registry::get().register_entry(MAKE_HW_EVENT(controller::on_channel_start), "dma::start");
}

void controller::write_cnt_l(const usize idx, const u8 data) noexcept
{
    auto& channel = channels_[idx];
    channel.cnt.dst_control = to_enum<channel::control::address_control>((data >> 5_u8) & 0b11_u8);
    channel.cnt.src_control = to_enum<channel::control::address_control>(
      (from_enum<u8>(channel.cnt.src_control) & 0b10_u8) | bit::extract(data, 7_u8));
}

void controller::write_cnt_h(const usize idx, const u8 data) noexcept
{
    auto& channel = channels_[idx];
    const bool was_enabled = channel.cnt.enabled;

    channel.cnt.enabled = bit::test(data, 7_u8);
    channel.cnt.irq = bit::test(data, 6_u8);
    channel.cnt.when = to_enum<channel::control::timing>(((data >> 4_u8) & 0b11_u8));
    channel.cnt.drq = idx == 3_usize && bit::test(data, 3_u8);
    channel.cnt.size = to_enum<channel::control::transfer_size>(bit::extract(data, 2_u8));
    channel.cnt.repeat = channel.cnt.when != channel::control::timing::immediately && bit::test(data, 1_u8);
    channel.cnt.src_control = to_enum<channel::control::address_control>(
      bit::extract(from_enum<u8>(channel.cnt.src_control), 0_u8) | (bit::extract(data, 0_u8) << 1_u8));

    if(!channel.cnt.enabled) {
        scheduled_channels_.erase(
          std::remove(scheduled_channels_.begin(), scheduled_channels_.end(), &channel),
          scheduled_channels_.end());
        running_channels_.erase(
          std::remove(running_channels_.begin(), running_channels_.end(), &channel),
          running_channels_.end());

        scheduler_->remove_event(channel.last_event_handle);
        return;
    }

    if(was_enabled) {
        return;
    }

    if(addr_in_rom_area(channel.src)) {
        channel.cnt.src_control = channel::control::address_control::increment;
    }

    latch(channel, false, is_for_fifo(&channel));
    schedule(channel, channel::control::timing::immediately);
}

void controller::run_channels() noexcept
{
    is_running_ = true;

    bool first_run = true;
    while(!running_channels_.empty()) {
        channel* channel = running_channels_.back(); // always has the highest priority

        if(UNLIKELY(first_run && (!addr_in_rom_area(channel->src) || !addr_in_rom_area(channel->dst)))) {
            first_run = false;
            bus_->idle();
            bus_->idle();
        }

        const bool for_fifo = is_for_fifo(channel);

        channel::control::transfer_size size = channel->cnt.size;
        channel::control::address_control src_control = channel->cnt.src_control;
        channel::control::address_control dst_control = channel->cnt.dst_control;

        if(for_fifo) {
            size = channel::control::transfer_size::word;
            dst_control = channel::control::address_control::fixed;
        }

        switch(size) {
            case channel::control::transfer_size::hword: {
                if(LIKELY(channel->internal.src >= 0x0200'0000_u32)) {
                    const u16 data = bus_->read_16(channel->internal.src, channel->next_access_type);
                    channel->latch = (widen<u32>(data) << 16_u32) | data;
                    latch_ = channel->latch;
                } else {
                    bus_->idle();
                }

                bus_->write_16(channel->internal.dst, narrow<u16>(channel->latch), channel->next_access_type);

                static constexpr array modify_offsets{2_i32, -2_i32, 0_i32, 2_i32};
                channel->internal.src += modify_offsets[from_enum<u32>(src_control)];
                channel->internal.dst += modify_offsets[from_enum<u32>(dst_control)];
                break;
            }
            case channel::control::transfer_size::word: {
                if(LIKELY(channel->internal.src >= 0x0200'0000_u32)) {
                    channel->latch = bus_->read_32(channel->internal.src, channel->next_access_type);
                    latch_ = channel->latch;
                } else {
                    bus_->idle();
                }

                bus_->write_32(channel->internal.dst, channel->latch, channel->next_access_type);

                static constexpr array modify_offsets{4_i32, -4_i32, 0_i32, 4_i32};
                channel->internal.src += modify_offsets[from_enum<u32>(src_control)];
                channel->internal.dst += modify_offsets[from_enum<u32>(dst_control)];
                break;
            }
        }

        --channel->internal.count;
        channel->next_access_type = cpu::mem_access::seq;

        if(channel->internal.count == 0_u32) {
            running_channels_.erase(std::find(running_channels_.begin(), running_channels_.end(), channel));

            if(channel->cnt.irq) {
                irq_.request_interrupt(to_enum<cpu::interrupt_source>(
                  from_enum<u32>(cpu::interrupt_source::dma_0) << channel->id));
            }

            if(channel->cnt.repeat) {
                latch(*channel, true, for_fifo);
            } else {
                channel->cnt.enabled = false;
            }
        }
    }

    is_running_ = false;
}

void controller::request(const occasion occasion) noexcept
{
    constexpr u32 fifo_addr_a = 0x0400'00A0_u32;
    constexpr u32 fifo_addr_b = 0x0400'00A4_u32;

    switch(occasion) {
        case occasion::vblank:
            for(channel& channel : channels_) {
                schedule(channel, channel::control::timing::vblank);
            }
            break;
        case occasion::hblank:
            for(channel& channel : channels_) {
                schedule(channel, channel::control::timing::hblank);
            }
            break;
        case occasion::video:
            schedule(channels_[3_usize], channel::control::timing::special);
            break;
        case occasion::fifo_a:
        case occasion::fifo_b:
            for(u32 n : range(1_u32, 3_u32)) {
                channel& channel = channels_[n];

                if((occasion == occasion::fifo_a && channel.dst == fifo_addr_a)
                  || (occasion == occasion::fifo_b && channel.dst == fifo_addr_b)) {
                    schedule(channel, channel::control::timing::special);
                }
            }
            break;
    }
}

void controller::latch(channel& channel, const bool for_repeat, const bool for_fifo) noexcept
{
    const data& masks = channel_masks[channel.id];

    u32 count;
    if(for_fifo) {
        channel.cnt.size = channel::control::transfer_size::word;
        count = 4_u32;
    } else {
        count = channel.count & masks.count;
        if(count == 0_u32) {
            count = masks.count + 1_u32;
        }
    }

    channel.next_access_type = cpu::mem_access::non_seq;

    static constexpr array alignment_masks{0b1_u32, 0b11_u32};
    if(for_repeat) {
        channel.internal.count = count;
        if(channel.cnt.dst_control == channel::control::address_control::inc_reload && !for_fifo) {
            channel.internal.dst = mask::clear(channel.dst, alignment_masks[from_enum<u32>(channel.cnt.size)]);
        }
    } else {
        channel.internal = data{
          mask::clear(channel.src, alignment_masks[from_enum<u32>(channel.cnt.size)]),
          mask::clear(channel.dst, alignment_masks[from_enum<u32>(channel.cnt.size)]),
          count
        };
    }
}

void controller::on_channel_start(const u32 /*late_cycles*/) noexcept
{
    channel* ch = scheduled_channels_.front();
    scheduled_channels_.erase(scheduled_channels_.begin());
    running_channels_.push_back(ch);
    sort_by_priority(running_channels_);
}

void controller::schedule(channel& channel, const channel::control::timing timing) noexcept
{
    if(channel.cnt.enabled && channel.cnt.when == timing && UNLIKELY(channel.cnt.src_control != channel::control::address_control::inc_reload)) {
        if(std::find(running_channels_.begin(), running_channels_.end(), &channel) == running_channels_.end()) {
            channel.last_event_handle = scheduler_->add_hw_event(2_u32, MAKE_HW_EVENT(controller::on_channel_start));
            scheduled_channels_.push_back(&channel);
        }
    }
}

void controller::serialize(archive& archive) const noexcept
{
    vector<u32> schedules_indices{scheduled_channels_.size()};
    std::transform(scheduled_channels_.begin(), scheduled_channels_.end(), schedules_indices.begin(), [](const channel* c) {
        return c->id;
    });
    archive.serialize(schedules_indices);
    for(const channel& c : channels_) {
        archive.serialize(c);
    }
    archive.serialize(latch_);
}

void controller::deserialize(const archive& archive) noexcept
{
    scheduled_channels_.clear();
    const vector<u32> schedules_indices = archive.deserialize<vector<u32>>();
    std::transform(schedules_indices.begin(), schedules_indices.end(), std::back_inserter(scheduled_channels_), [&](const u32 idx) {
        return &channels_[idx];
    });
    for(channel& c : channels_) {
        archive.deserialize(c);
    }
    archive.deserialize(latch_);
}

} // namespace gba::dma

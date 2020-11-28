#include <gba/cartridge/gamepak_db.h>

namespace gba {

// taken from https://github.com/fleroviux/NanoboyAdvance/blob/master/source/emulator/cartridge/game_db.cpp
constexpr array pak_db {
  pak_db_entry{"ALFP", backup::type::eeprom_64, false, false}, /* Dragon Ball Z - The Legacy of Goku II (Europe)(En,Fr,De,Es,It) */
  pak_db_entry{"ALGP", backup::type::eeprom_64, false, false}, /* Dragon Ball Z - The Legacy of Goku (Europe)(En,Fr,De,Es,It) */
  pak_db_entry{"AROP", backup::type::eeprom_64, false, false}, /* Rocky (Europe)(En,Fr,De,Es,It) */
  pak_db_entry{"AR8e", backup::type::eeprom_64, false, false}, /* Rocky (USA)(En,Fr,De,Es,It) */
  pak_db_entry{"AXVE", backup::type::flash_128, true, false},  /* Pokemon - Ruby Version (USA, Europe) */
  pak_db_entry{"AXPE", backup::type::flash_128, true, false},  /* Pokemon - Sapphire Version (USA, Europe) */
  pak_db_entry{"AX4P", backup::type::flash_128, false, false}, /* Super Mario Advance 4 - Super Mario Bros. 3 (Europe)(En,Fr,De,Es,It) */
  pak_db_entry{"A2YE", backup::type::none, false, false},        /* Top Gun - Combat Zones (USA)(En,Fr,De,Es,It) */
  pak_db_entry{"BDBP", backup::type::eeprom_64, false, false}, /* Dragon Ball Z - Taiketsu (Europe)(En,Fr,De,Es,It) */
  pak_db_entry{"BM5P", backup::type::flash_64, false, false},  /* Mario vs. Donkey Kong (Europe) */
  pak_db_entry{"BPEE", backup::type::flash_128, true, false},  /* Pokemon - Emerald Version (USA, Europe) */
  pak_db_entry{"BY6P", backup::type::sram, false, false},        /* Yu-Gi-Oh! - Ultimate Masters - World Championship Tournament 2006 (Europe)(En,Jp,Fr,De,Es,It) */
  pak_db_entry{"B24E", backup::type::flash_128, false, false}, /* Pokemon Mystery Dungeon - Red Rescue Team (USA, Australia) */
  pak_db_entry{"FADE", backup::type::eeprom_4, false, true},   /* Classic NES Series - Castlevania (USA, Europe) */
  pak_db_entry{"FBME", backup::type::eeprom_4, false, true},   /* Classic NES Series - Bomberman (USA, Europe) */
  pak_db_entry{"FDKE", backup::type::eeprom_4, false, true},   /* Classic NES Series - Donkey Kong (USA, Europe) */
  pak_db_entry{"FDME", backup::type::eeprom_4, false, true},   /* Classic NES Series - Dr. Mario (USA, Europe) */
  pak_db_entry{"FEBE", backup::type::eeprom_4, false, true},   /* Classic NES Series - Excitebike (USA, Europe) */
  pak_db_entry{"FICE", backup::type::eeprom_4, false, true},   /* Classic NES Series - Ice Climber (USA, Europe) */
  pak_db_entry{"FLBE", backup::type::eeprom_4, false, true},   /* Classic NES Series - Zelda II - The Adventure of Link (USA, Europe) */
  pak_db_entry{"FMRE", backup::type::eeprom_4, false, true},   /* Classic NES Series - Metroid (USA, Europe) */
  pak_db_entry{"FP7E", backup::type::eeprom_4, false, true},   /* Classic NES Series - Pac-Man (USA, Europe) */
  pak_db_entry{"FSME", backup::type::eeprom_4, false, true},   /* Classic NES Series - Super Mario Bros. (USA, Europe) */
  pak_db_entry{"FXVE", backup::type::eeprom_4, false, true},   /* Classic NES Series - Xevious (USA, Europe) */
  pak_db_entry{"FZLE", backup::type::eeprom_64, false, true},  /* Classic NES Series - Legend of Zelda (USA, Europe) */
  pak_db_entry{"KYGP", backup::type::eeprom_64, false, false}, /* Yoshi's Universal Gravitation (Europe)(En,Fr,De,Es,It) */
  pak_db_entry{"U3IP", backup::type::detect, true, false},       /* Boktai - The Sun Is in Your Hand (Europe)(En,Fr,De,Es,It) */
  pak_db_entry{"U32P", backup::type::detect, true, false},       /* Boktai 2 - Solar Boy Django (Europe)(En,Fr,De,Es,It) */
  pak_db_entry{"AGFE", backup::type::flash_64, true, false},   /* Golden Sun - The Lost Age (USA) */
  pak_db_entry{"AGSE", backup::type::flash_64, true, false},   /* Golden Sun (USA) */
  pak_db_entry{"ALFE", backup::type::eeprom_64, false, false}, /* Dragon Ball Z - The Legacy of Goku II (USA) */
  pak_db_entry{"ALGE", backup::type::eeprom_64, false, false}, /* Dragon Ball Z - The Legacy of Goku (USA) */
  pak_db_entry{"AX4E", backup::type::flash_128, false, false}, /* Super Mario Advance 4 - Super Mario Bros 3 - Super Mario Advance 4 v1.1 (USA) */
  pak_db_entry{"BDBE", backup::type::eeprom_64, false, false}, /* Dragon Ball Z - Taiketsu (USA) */
  pak_db_entry{"BG3E", backup::type::eeprom_64, false, false}, /* Dragon Ball Z - Buu's Fury (USA) */
  pak_db_entry{"BLFE", backup::type::eeprom_64, false, false}, /* 2 Games in 1 - Dragon Ball Z - The Legacy of Goku I & II (USA) */
  pak_db_entry{"BPRE", backup::type::flash_128, false, false}, /* Pokemon - Fire Red Version (USA, Europe) */
  pak_db_entry{"BPGE", backup::type::flash_128, false, false}, /* Pokemon - Leaf Green Version (USA, Europe) */
  pak_db_entry{"BT4E", backup::type::eeprom_64, false, false}, /* Dragon Ball GT - Transformation (USA) */
  pak_db_entry{"BUFE", backup::type::eeprom_64, false, false}, /* 2 Games in 1 - Dragon Ball Z - Buu's Fury + Dragon Ball GT - Transformation (USA) */
  pak_db_entry{"BYGE", backup::type::sram, false, false},        /* Yu-Gi-Oh! GX - Duel Academy (USA) */
  pak_db_entry{"KYGE", backup::type::eeprom_64, false, false}, /* Yoshi - Topsy-Turvy (USA) */
  pak_db_entry{"PSAE", backup::type::flash_128, false, false}, /* e-Reader (USA) */
  pak_db_entry{"U3IE", backup::type::detect, true, false},       /* Boktai - The Sun Is in Your Hand (USA) */
  pak_db_entry{"U32E", backup::type::detect, true, false},       /* Boktai 2 - Solar Boy Django (USA) */
  pak_db_entry{"ALFJ", backup::type::eeprom_64, false, false}, /* Dragon Ball Z - The Legacy of Goku II International (Japan) */
  pak_db_entry{"AXPJ", backup::type::flash_128, true, false},  /* Pocket Monsters - Sapphire (Japan) */
  pak_db_entry{"AXVJ", backup::type::flash_128, true, false},  /* Pocket Monsters - Ruby (Japan) */
  pak_db_entry{"AX4J", backup::type::flash_128, false, false}, /* Super Mario Advance 4 (Japan) */
  pak_db_entry{"BFTJ", backup::type::flash_128, false, false}, /* F-Zero - Climax (Japan) */
  pak_db_entry{"BGWJ", backup::type::flash_128, false, false}, /* Game Boy Wars Advance 1+2 (Japan) */
  pak_db_entry{"BKAJ", backup::type::flash_128, true, false},  /* Sennen Kazoku (Japan) */
  pak_db_entry{"BPEJ", backup::type::flash_128, true, false},  /* Pocket Monsters - Emerald (Japan) */
  pak_db_entry{"BPGJ", backup::type::flash_128, false, false}, /* Pocket Monsters - Leaf Green (Japan) */
  pak_db_entry{"BPRJ", backup::type::flash_128, false, false}, /* Pocket Monsters - Fire Red (Japan) */
  pak_db_entry{"BDKJ", backup::type::eeprom_64, false, false}, /* Digi Communication 2 - Datou! Black Gemagema Dan (Japan) */
  pak_db_entry{"BR4J", backup::type::detect, true, false},       /* Rockman EXE 4.5 - Real Operation (Japan) */
  pak_db_entry{"FMBJ", backup::type::eeprom_4, false, true},   /* Famicom Mini Vol. 01 - Super Mario Bros. (Japan) */
  pak_db_entry{"FCLJ", backup::type::eeprom_4, false, true},   /* Famicom Mini Vol. 12 - Clu Clu Land (Japan) */
  pak_db_entry{"FBFJ", backup::type::eeprom_4, false, true},   /* Famicom Mini Vol. 13 - Balloon Fight (Japan) */
  pak_db_entry{"FWCJ", backup::type::eeprom_4, false, true},   /* Famicom Mini Vol. 14 - Wrecking Crew (Japan) */
  pak_db_entry{"FDMJ", backup::type::eeprom_4, false, true},   /* Famicom Mini Vol. 15 - Dr. Mario (Japan) */
  pak_db_entry{"FDDJ", backup::type::eeprom_4, false, true},   /* Famicom Mini Vol. 16 - Dig Dug (Japan) */
  pak_db_entry{"FTBJ", backup::type::eeprom_4, false, true},   /* Famicom Mini Vol. 17 - Takahashi Meijin no Boukenjima (Japan) */
  pak_db_entry{"FMKJ", backup::type::eeprom_4, false, true},   /* Famicom Mini Vol. 18 - Makaimura (Japan) */
  pak_db_entry{"FTWJ", backup::type::eeprom_4, false, true},   /* Famicom Mini Vol. 19 - Twin Bee (Japan) */
  pak_db_entry{"FGGJ", backup::type::eeprom_4, false, true},   /* Famicom Mini Vol. 20 - Ganbare Goemon! Karakuri Douchuu (Japan) */
  pak_db_entry{"FM2J", backup::type::eeprom_4, false, true},   /* Famicom Mini Vol. 21 - Super Mario Bros. 2 (Japan) */
  pak_db_entry{"FNMJ", backup::type::eeprom_4, false, true},   /* Famicom Mini Vol. 22 - Nazo no Murasame Jou (Japan) */
  pak_db_entry{"FMRJ", backup::type::eeprom_4, false, true},   /* Famicom Mini Vol. 23 - Metroid (Japan) */
  pak_db_entry{"FPTJ", backup::type::eeprom_4, false, true},   /* Famicom Mini Vol. 24 - Hikari Shinwa - Palthena no Kagami (Japan) */
  pak_db_entry{"FLBJ", backup::type::eeprom_4, false, true},   /* Famicom Mini Vol. 25 - The Legend of Zelda 2 - Link no Bouken (Japan) */
  pak_db_entry{"FFMJ", backup::type::eeprom_4, false, true},   /* Famicom Mini Vol. 26 - Famicom Mukashi Banashi - Shin Onigashima - Zen Kou Hen (Japan) */
  pak_db_entry{"FTKJ", backup::type::eeprom_4, false, true},   /* Famicom Mini Vol. 27 - Famicom Tantei Club - Kieta Koukeisha - Zen Kou Hen (Japan) */
  pak_db_entry{"FTUJ", backup::type::eeprom_4, false, true},   /* Famicom Mini Vol. 28 - Famicom Tantei Club Part II - Ushiro ni Tatsu Shoujo - Zen Kou Hen (Japan) */
  pak_db_entry{"FADJ", backup::type::eeprom_4, false, true},   /* Famicom Mini Vol. 29 - Akumajou Dracula (Japan) */
  pak_db_entry{"FSDJ", backup::type::eeprom_4, false, true},   /* Famicom Mini Vol. 30 - SD Gundam World - Gachapon Senshi Scramble Wars (Japan) */
  pak_db_entry{"KHPJ", backup::type::eeprom_64, false, false}, /* Koro Koro Puzzle - Happy Panechu! (Japan) */
  pak_db_entry{"KYGJ", backup::type::eeprom_64, false, false}, /* Yoshi no Banyuuinryoku (Japan) */
  pak_db_entry{"PSAJ", backup::type::flash_128, false, false}, /* Card e-Reader+ (Japan) */
  pak_db_entry{"U3IJ", backup::type::detect, true, false},       /* Bokura no Taiyou - Taiyou Action RPG (Japan) */
  pak_db_entry{"U32J", backup::type::detect, true, false},       /* Zoku Bokura no Taiyou - Taiyou Shounen Django (Japan) */
  pak_db_entry{"U33J", backup::type::detect, true, false},       /* Shin Bokura no Taiyou - Gyakushuu no Sabata (Japan) */
  pak_db_entry{"AXPF", backup::type::flash_128, true, false},  /* Pokemon - Version Saphir (France) */
  pak_db_entry{"AXVF", backup::type::flash_128, true, false},  /* Pokemon - Version Rubis (France) */
  pak_db_entry{"BPEF", backup::type::flash_128, true, false},  /* Pokemon - Version Emeraude (France) */
  pak_db_entry{"BPGF", backup::type::flash_128, false, false}, /* Pokemon - Version Vert Feuille (France) */
  pak_db_entry{"BPRF", backup::type::flash_128, false, false}, /* Pokemon - Version Rouge Feu (France) */
  pak_db_entry{"AXPI", backup::type::flash_128, true, false},  /* Pokemon - Versione Zaffiro (Italy) */
  pak_db_entry{"AXVI", backup::type::flash_128, true, false},  /* Pokemon - Versione Rubino (Italy) */
  pak_db_entry{"BPEI", backup::type::flash_128, true, false},  /* Pokemon - Versione Smeraldo (Italy) */
  pak_db_entry{"BPGI", backup::type::flash_128, false, false}, /* Pokemon - Versione Verde Foglia (Italy) */
  pak_db_entry{"BPRI", backup::type::flash_128, false, false}, /* Pokemon - Versione Rosso Fuoco (Italy) */
  pak_db_entry{"AXPD", backup::type::flash_128, true, false},  /* Pokemon - Saphir-Edition (Germany) */
  pak_db_entry{"AXVD", backup::type::flash_128, true, false},  /* Pokemon - Rubin-Edition (Germany) */
  pak_db_entry{"BPED", backup::type::flash_128, true, false},  /* Pokemon - Smaragd-Edition (Germany) */
  pak_db_entry{"BPGD", backup::type::flash_128, false, false}, /* Pokemon - Blattgruene Edition (Germany) */
  pak_db_entry{"BPRD", backup::type::flash_128, false, false}, /* Pokemon - Feuerrote Edition (Germany) */
  pak_db_entry{"AXPS", backup::type::flash_128, true, false},  /* Pokemon - Edicion Zafiro (Spain) */
  pak_db_entry{"AXVS", backup::type::flash_128, true, false},  /* Pokemon - Edicion Rubi (Spain) */
  pak_db_entry{"BPES", backup::type::flash_128, true, false},  /* Pokemon - Edicion Esmeralda (Spain) */
  pak_db_entry{"BPGS", backup::type::flash_128, false, false}, /* Pokemon - Edicion Verde Hoja (Spain) */
  pak_db_entry{"BPRS", backup::type::flash_128, false, false}  /* Pokemon - Edicion Rojo Fuego (Spain) */,
  pak_db_entry{"A9DP", backup::type::eeprom_4, false, false},  /* DOOM II */
  pak_db_entry{"AAOJ", backup::type::eeprom_4, false, false},  /* Acrobat Kid (Japan) */
  pak_db_entry{"BGDP", backup::type::eeprom_4, false, false},  /* Baldur's Gate - Dark Alliance (Europe) */
  pak_db_entry{"BGDE", backup::type::eeprom_4, false, false},  /* Baldur's Gate - Dark Alliance (USA) */
  pak_db_entry{"BJBE", backup::type::eeprom_4, false, false},  /* 007 - Everything or Nothing (USA, Europe) (En,Fr,De) */
  pak_db_entry{"BJBJ", backup::type::eeprom_4, false, false},  /* 007 - Everything or Nothing (Japan) */
  pak_db_entry{"ALUP", backup::type::eeprom_4, false, false},  /* 0937 - Super Monkey Ball Jr. (Europe) */
  pak_db_entry{"ALUE", backup::type::eeprom_4, false, false}   /* 0763 - Super Monkey Ball Jr. (USA) */
};

std::optional<pak_db_entry> query_pak_db(const std::string_view game_code) noexcept
{
    const auto* it = std::find_if(pak_db.cbegin(), pak_db.cend(), [game_code](const pak_db_entry& entry) {
        return entry.game_code == game_code;
    });

    return it == pak_db.cend()
      ? std::nullopt
      : std::optional<pak_db_entry>{*it};
}

} // namespace gba

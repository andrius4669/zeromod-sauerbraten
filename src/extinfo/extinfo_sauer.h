namespace sauer
{
    enum { GUN_FIST = 0, GUN_SG, GUN_CG, GUN_RL, GUN_RIFLE, GUN_GL, GUN_PISTOL, GUN_FIREBALL, GUN_ICEBALL, GUN_SLIMEBALL, GUN_BITE, GUN_BARREL, NUMGUNS };
    enum { A_BLUE, A_GREEN, A_YELLOW };     // armour types... take 20/40/60 % off

    enum
    {
        M_TEAM       = 1<<0,
        M_NOITEMS    = 1<<1,
        M_NOAMMO     = 1<<2,
        M_INSTA      = 1<<3,
        M_EFFICIENCY = 1<<4,
        M_TACTICS    = 1<<5,
        M_CAPTURE    = 1<<6,
        M_REGEN      = 1<<7,
        M_CTF        = 1<<8,
        M_PROTECT    = 1<<9,
        M_HOLD       = 1<<10,
        M_OVERTIME   = 1<<11,
        M_EDIT       = 1<<12,
        M_DEMO       = 1<<13,
        M_LOCAL      = 1<<14,
        M_LOBBY      = 1<<15,
        M_DMSP       = 1<<16,
        M_CLASSICSP  = 1<<17,
        M_SLOWMO     = 1<<18,
        M_COLLECT    = 1<<19
    };

    static struct gamemodeinfo
    {
        const char *name;
        int flags;
        const char *info;
    } gamemodes[] =
    {
        { "SP", M_LOCAL | M_CLASSICSP, NULL },
        { "DMSP", M_LOCAL | M_DMSP, NULL },
        { "demo", M_DEMO | M_LOCAL, NULL},
        { "ffa", M_LOBBY, "Free For All: Collect items for ammo. Frag everyone to score points." },
        { "coop edit", M_EDIT, "Cooperative Editing: Edit maps with multiple players simultaneously." },
        { "teamplay", M_TEAM, "Teamplay: Collect items for ammo. Frag \fs\f3the enemy team\fr to score points for \fs\f1your team\fr." },
        { "instagib", M_NOITEMS | M_INSTA, "Instagib: You spawn with full rifle ammo and die instantly from one shot. There are no items. Frag everyone to score points." },
        { "insta team", M_NOITEMS | M_INSTA | M_TEAM, "Instagib Team: You spawn with full rifle ammo and die instantly from one shot. There are no items. Frag \fs\f3the enemy team\fr to score points for \fs\f1your team\fr." },
        { "efficiency", M_NOITEMS | M_EFFICIENCY, "Efficiency: You spawn with all weapons and armour. There are no items. Frag everyone to score points." },
        { "effic team", M_NOITEMS | M_EFFICIENCY | M_TEAM, "Efficiency Team: You spawn with all weapons and armour. There are no items. Frag \fs\f3the enemy team\fr to score points for \fs\f1your team\fr." },
        { "tactics", M_NOITEMS | M_TACTICS, "Tactics: You spawn with two random weapons and armour. There are no items. Frag everyone to score points." },
        { "tac team", M_NOITEMS | M_TACTICS | M_TEAM, "Tactics Team: You spawn with two random weapons and armour. There are no items. Frag \fs\f3the enemy team\fr to score points for \fs\f1your team\fr." },
        { "capture", M_NOAMMO | M_TACTICS | M_CAPTURE | M_TEAM, "Capture: Capture neutral bases or steal \fs\f3enemy bases\fr by standing next to them.  \fs\f1Your team\fr scores points for every 10 seconds it holds a base. You spawn with two random weapons and armour. Collect extra ammo that spawns at \fs\f1your bases\fr. There are no ammo items." },
        { "regen capture", M_NOITEMS | M_CAPTURE | M_REGEN | M_TEAM, "Regen Capture: Capture neutral bases or steal \fs\f3enemy bases\fr by standing next to them. \fs\f1Your team\fr scores points for every 10 seconds it holds a base. Regenerate health and ammo by standing next to \fs\f1your bases\fr. There are no items." },
        { "ctf", M_CTF | M_TEAM, "Capture The Flag: Capture \fs\f3the enemy flag\fr and bring it back to \fs\f1your flag\fr to score points for \fs\f1your team\fr. Collect items for ammo." },
        { "insta ctf", M_NOITEMS | M_INSTA | M_CTF | M_TEAM, "Instagib Capture The Flag: Capture \fs\f3the enemy flag\fr and bring it back to \fs\f1your flag\fr to score points for \fs\f1your team\fr. You spawn with full rifle ammo and die instantly from one shot. There are no items." },
        { "protect", M_CTF | M_PROTECT | M_TEAM, "Protect The Flag: Touch \fs\f3the enemy flag\fr to score points for \fs\f1your team\fr. Pick up \fs\f1your flag\fr to protect it. \fs\f1Your team\fr loses points if a dropped flag resets. Collect items for ammo." },
        { "insta protect", M_NOITEMS | M_INSTA | M_CTF | M_PROTECT | M_TEAM, "Instagib Protect The Flag: Touch \fs\f3the enemy flag\fr to score points for \fs\f1your team\fr. Pick up \fs\f1your flag\fr to protect it. \fs\f1Your team\fr loses points if a dropped flag resets. You spawn with full rifle ammo and die instantly from one shot. There are no items." },
        { "hold", M_CTF | M_HOLD | M_TEAM, "Hold The Flag: Hold \fs\f7the flag\fr for 20 seconds to score points for \fs\f1your team\fr. Collect items for ammo." },
        { "insta hold", M_NOITEMS | M_INSTA | M_CTF | M_HOLD | M_TEAM, "Instagib Hold The Flag: Hold \fs\f7the flag\fr for 20 seconds to score points for \fs\f1your team\fr. You spawn with full rifle ammo and die instantly from one shot. There are no items." },
        { "effic ctf", M_NOITEMS | M_EFFICIENCY | M_CTF | M_TEAM, "Efficiency Capture The Flag: Capture \fs\f3the enemy flag\fr and bring it back to \fs\f1your flag\fr to score points for \fs\f1your team\fr. You spawn with all weapons and armour. There are no items." },
        { "effic protect", M_NOITEMS | M_EFFICIENCY | M_CTF | M_PROTECT | M_TEAM, "Efficiency Protect The Flag: Touch \fs\f3the enemy flag\fr to score points for \fs\f1your team\fr. Pick up \fs\f1your flag\fr to protect it. \fs\f1Your team\fr loses points if a dropped flag resets. You spawn with all weapons and armour. There are no items." },
        { "effic hold", M_NOITEMS | M_EFFICIENCY | M_CTF | M_HOLD | M_TEAM, "Efficiency Hold The Flag: Hold \fs\f7the flag\fr for 20 seconds to score points for \fs\f1your team\fr. You spawn with all weapons and armour. There are no items." },
        { "collect", M_COLLECT | M_TEAM, "Skull Collector: Frag \fs\f3the enemy team\fr to drop \fs\f3skulls\fr. Collect them and bring them to \fs\f3the enemy base\fr to score points for \fs\f1your team\fr or steal back \fs\f1your skulls\fr. Collect items for ammo." },
        { "insta collect", M_NOITEMS | M_INSTA | M_COLLECT | M_TEAM, "Instagib Skull Collector: Frag \fs\f3the enemy team\fr to drop \fs\f3skulls\fr. Collect them and bring them to \fs\f3the enemy base\fr to score points for \fs\f1your team\fr or steal back \fs\f1your skulls\fr. You spawn with full rifle ammo and die instantly from one shot. There are no items." },
        { "effic collect", M_NOITEMS | M_EFFICIENCY | M_COLLECT | M_TEAM, "Efficiency Skull Collector: Frag \fs\f3the enemy team\fr to drop \fs\f3skulls\fr. Collect them and bring them to \fs\f3the enemy base\fr to score points for \fs\f1your team\fr or steal back \fs\f1your skulls\fr. You spawn with all weapons and armour. There are no items." }
    };

    static const int STARTGAMEMODE = -3;
    static const int NUMGAMEMODES = sizeof(gamemodes)/sizeof(gamemodes[0]);

    static inline bool m_valid(int mode) { return mode >= STARTGAMEMODE && mode < STARTGAMEMODE + NUMGAMEMODES; }
    static inline bool m_check(int mode, int flag) { return m_valid(mode) && (gamemodes[mode - STARTGAMEMODE].flags & flag); }
    static inline bool m_checknot(int mode, int flag) { return m_valid(mode) && !(gamemodes[mode - STARTGAMEMODE].flags & flag); }
    static inline bool m_checkall(int mode, int flag) { return m_valid(mode) && (gamemodes[mode - STARTGAMEMODE].flags & flag) == flag; }
}


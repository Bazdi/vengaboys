#include "champion_spell_names.hpp"

namespace champion_spell_names {

    extern std::map<std::string, std::map<char, std::string>> spell_names_data;

    void initialize_spell_names() {
        if (!spell_names_data.empty()) {
            return;
        }

        spell_names_data["Aatrox"] = {
            {'P', "Deathbringer Stance"},
            {'Q', "The Darkin Blade"},
            {'W', "Infernal Chains"},
            {'E', "Umbral Dash"},
            {'R', "World Ender"}
        };
        spell_names_data["Ahri"] = {
            {'P', "Essence Theft"},
            {'Q', "Orb of Deception"},
            {'W', "Fox-Fire"},
            {'E', "Charm"},
            {'R', "Spirit Rush"}
        };
        spell_names_data["Akali"] = {
            {'P', "Assassin's Mark"},
            {'Q', "Five Point Strike"},
            {'W', "Twilight Shroud"},
            {'E', "Shuriken Flip"},
            {'R', "Perfect Execution"}
        };
        spell_names_data["Akshan"] = {
            {'P', "Dirty Fighting"},
            {'Q', "Avengerang"},
            {'W', "Going Rogue"},
            {'E', "Heroic Swing"},
            {'R', "Comeuppance"}
        };
        spell_names_data["Alistar"] = {
            {'P', "Triumphant Roar"},
            {'Q', "Pulverize"},
            {'W', "Headbutt"},
            {'E', "Trample"},
            {'R', "Unbreakable Will"}
        };
        spell_names_data["Amumu"] = {
            {'P', "Cursed Touch"},
            {'Q', "Bandage Toss"},
            {'W', "Despair"},
            {'E', "Tantrum"},
            {'R', "Curse of the Sad Mummy"}
        };
        spell_names_data["Anivia"] = {
            {'P', "Rebirth"},
            {'Q', "Flash Frost"},
            {'W', "Crystallize"},
            {'E', "Frostbite"},
            {'R', "Glacial Storm"}
        };
        spell_names_data["Annie"] = {
            {'P', "Pyromania"},
            {'Q', "Disintegrate"},
            {'W', "Incinerate"},
            {'E', "Molten Shield"},
            {'R', "Summon: Tibbers"}
        };
        spell_names_data["Aphelios"] = {
            {'P', "The Hitman and the Seer"},
            {'Q', "Weapon Abilities"},
            {'W', "Phase"},
            {'E', "No E"},
            {'R', "Moonlight Vigil"}
        };
        spell_names_data["Ashe"] = {
            {'P', "Frost Shot"},
            {'Q', "Ranger's Focus"},
            {'W', "Volley"},
            {'E', "Hawkshot"},
            {'R', "Enchanted Crystal Arrow"}
        };
        spell_names_data["Aurelion Sol"] = {
            {'P', "Cosmic Creator"},
            {'Q', "Breath of Light"},
            {'W', "Astral Flight"},
            {'E', "Singularity"},
            {'R', "Falling Star / The Skies Descend"}
        };
        spell_names_data["Azir"] = {
            {'P', "Shurima's Legacy"},
            {'Q', "Conquering Sands"},
            {'W', "Arise!"},
            {'E', "Shifting Sands"},
            {'R', "Emperor's Divide"}
        };
        spell_names_data["Bard"] = {
            {'P', "Traveler's Call"},
            {'Q', "Cosmic Binding"},
            {'W', "Caretaker's Shrine"},
            {'E', "Magical Journey"},
            {'R', "Tempered Fate"}
        };
        spell_names_data["Bel'Veth"] = {
            {'P', "Death in Lavender"},
            {'Q', "Void Surge"},
            {'W', "Above and Below"},
            {'E', "Royal Maelstrom"},
            {'R', "Endless Banquet"}
        };
        spell_names_data["Blitzcrank"] = {
            {'P', "Mana Barrier"},
            {'Q', "Rocket Grab"},
            {'W', "Overdrive"},
            {'E', "Power Fist"},
            {'R', "Static Field"}
        };
        spell_names_data["Brand"] = {
            {'P', "Blaze"},
            {'Q', "Sear"},
            {'W', "Pillar of Flame"},
            {'E', "Conflagration"},
            {'R', "Pyroclasm"}
        };
        spell_names_data["Braum"] = {
            {'P', "Concussive Blows"},
            {'Q', "Winter's Bite"},
            {'W', "Stand Behind Me"},
            {'E', "Unbreakable"},
            {'R', "Glacial Fissure"}
        };
        spell_names_data["Briar"] = {
              {'P', "Crimson Curse"},
              {'Q', "Head Rush"},
              {'W', "Blood Frenzy / Snack Attack"},
              {'E', "Chilling Scream"},
              {'R', "Certain Death"}
        };
        spell_names_data["Caitlyn"] = {
            {'P', "Headshot"},
            {'Q', "Piltover Peacemaker"},
            {'W', "Yordle Snap Trap"},
            {'E', "90 Caliber Net"},
            {'R', "Ace in the Hole"}
        };
        spell_names_data["Camille"] = {
            {'P', "Adaptive Defenses"},
            {'Q', "Precision Protocol"},
            {'W', "Tactical Sweep"},
            {'E', "Hookshot"},
            {'R', "The Hextech Ultimatum"}
        };
        spell_names_data["Cassiopeia"] = {
            {'P', "Serpentine Grace"},
            {'Q', "Noxious Blast"},
            {'W', "Miasma"},
            {'E', "Twin Fang"},
            {'R', "Petrifying Gaze"}
        };
        spell_names_data["Cho'Gath"] = {
            {'P', "Carnivore"},
            {'Q', "Rupture"},
            {'W', "Feral Scream"},
            {'E', "Vorpal Spikes"},
            {'R', "Feast"}
        };
        spell_names_data["Corki"] = {
            {'P', "Hextech Munitions"},
            {'Q', "Phosphorus Bomb"},
            {'W', "Valkyrie / Special Delivery"},
            {'E', "Gatling Gun"},
            {'R', "Missile Barrage"}
        };
        spell_names_data["Darius"] = {
            {'P', "Hemorrhage"},
            {'Q', "Decimate"},
            {'W', "Crippling Strike"},
            {'E', "Apprehend"},
            {'R', "Noxian Guillotine"}
        };
        spell_names_data["Diana"] = {
            {'P', "Moonsilver Blade"},
            {'Q', "Crescent Strike"},
            {'W', "Pale Cascade"},
            {'E', "Lunar Rush"},
            {'R', "Moonfall"}
        };
        spell_names_data["Dr. Mundo"] = {
            {'P', "Goes Where He Pleases"},
            {'Q', "Infected Bonesaw"},
            {'W', "Heart Zapper"},
            {'E', "Blunt Force Trauma"},
            {'R', "Maximum Dosage"}
        };
        spell_names_data["Draven"] = {
            {'P', "League of Draven"},
            {'Q', "Spinning Axe"},
            {'W', "Blood Rush"},
            {'E', "Stand Aside"},
            {'R', "Whirling Death"}
        };
        spell_names_data["Ekko"] = {
            {'P', "Z-Drive Resonance"},
            {'Q', "Timewinder"},
            {'W', "Parallel Convergence"},
            {'E', "Phase Dive"},
            {'R', "Chronobreak"}
        };
        spell_names_data["Elise"] = {
            {'P', "Spider Queen"},
            {'Q', "Neurotoxin / Venomous Bite"},
            {'W', "Volatile Spiderling / Skittering Frenzy"},
            {'E', "Cocoon / Rappel"},
            {'R', "Spider Form / Human Form"}
        };
        spell_names_data["Evelynn"] = {
            {'P', "Demon Shade"},
            {'Q', "Hate Spike"},
            {'W', "Allure"},
            {'E', "Whiplash"},
            {'R', "Last Caress"}
        };
        spell_names_data["Ezreal"] = {
            {'P', "Rising Spell Force"},
            {'Q', "Mystic Shot"},
            {'W', "Essence Flux"},
            {'E', "Arcane Shift"},
            {'R', "Trueshot Barrage"}
        };
        spell_names_data["Fiddlesticks"] = {
            {'P', "A Harmless Scarecrow"},
            {'Q', "Terrify"},
            {'W', "Bountiful Harvest"},
            {'E', "Reap"},
            {'R', "Crowstorm"}
        };
        spell_names_data["Fiora"] = {
            {'P', "Duelist's Dance"},
            {'Q', "Lunge"},
            {'W', "Riposte"},
            {'E', "Bladework"},
            {'R', "Grand Challenge"}
        };
        spell_names_data["Fizz"] = {
            {'P', "Nimble Fighter"},
            {'Q', "Urchin Strike"},
            {'W', "Seastone Trident"},
            {'E', "Playful / Trickster"},
            {'R', "Chum the Waters"}
        };
        spell_names_data["Galio"] = {
            {'P', "Colossal Smash"},
            {'Q', "Winds of War"},
            {'W', "Shield of Durand"},
            {'E', "Justice Punch"},
            {'R', "Hero's Entrance"}
        };
        spell_names_data["Gangplank"] = {
            {'P', "Trial by Fire"},
            {'Q', "Parrrley"},
            {'W', "Remove Scurvy"},
            {'E', "Powder Keg"},
            {'R', "Cannon Barrage"}
        };
        spell_names_data["Garen"] = {
            {'P', "Perseverance"},
            {'Q', "Decisive Strike"},
            {'W', "Courage"},
            {'E', "Judgment"},
            {'R', "Demacian Justice"}
        };
        spell_names_data["Gnar"] = {
            {'P', "Rage Gene"},
            {'Q', "Boomerang Throw / Boulder Toss"},
            {'W', "Hyper / Wallop"},
            {'E', "Hop / Crunch"},
            {'R', "GNAR!"}
        };
        spell_names_data["Gragas"] = {
            {'P', "Happy Hour"},
            {'Q', "Barrel Roll"},
            {'W', "Drunken Rage"},
            {'E', "Body Slam"},
            {'R', "Explosive Cask"}
        };
        spell_names_data["Graves"] = {
            {'P', "New Destiny"},
            {'Q', "End of the Line"},
            {'W', "Smoke Screen"},
            {'E', "Quickdraw"},
            {'R', "Collateral Damage"}
        };
        spell_names_data["Gwen"] = {
            {'P', "Thousand Cuts"},
            {'Q', "Snip Snip!"},
            {'W', "Hallowed Mist"},
            {'E', "Skip 'n Slash"},
            {'R', "Needlework"}
        };
        spell_names_data["Hecarim"] = {
            {'P', "Warpath"},
            {'Q', "Rampage"},
            {'W', "Spirit of Dread"},
            {'E', "Devastating Charge"},
            {'R', "Onslaught of Shadows"}
        };
        spell_names_data["Heimerdinger"] = {
            {'P', "Hextech Affinity"},
            {'Q', "H-28G Evolution Turret"},
            {'W', "Hextech Micro-Rockets"},
            {'E', "CH-2 Electron Storm Grenade"},
            {'R', "UPGRADE!!!"}
        };
        spell_names_data["Hwei"] = {
            {'P', "Signature of the Visionary"},
            {'Q', "Subject: Disaster"},
            {'W', "Subject: Serenity"},
            {'E', "Subject: Torment"},
            {'R', "Spiraling Despair"}
        };
        spell_names_data["Illaoi"] = {
            {'P', "Prophet of an Elder God"},
            {'Q', "Tentacle Smash"},
            {'W', "Harsh Lesson"},
            {'E', "Test of Spirit"},
            {'R', "Leap of Faith"}
        };
        spell_names_data["Irelia"] = {
            {'P', "Ionian Fervor"},
            {'Q', "Bladesurge"},
            {'W', "Defiant Dance"},
            {'E', "Flawless Duet"},
            {'R', "Vanguard's Edge"}
        };
        spell_names_data["Ivern"] = {
            {'P', "Friend of the Forest"},
            {'Q', "Rootcaller"},
            {'W', "Brushmaker"},
            {'E', "Triggerseed"},
            {'R', "Daisy!"}
        };
        spell_names_data["Janna"] = {
            {'P', "Tailwind"},
            {'Q', "Howling Gale"},
            {'W', "Zephyr"},
            {'E', "Eye Of The Storm"},
            {'R', "Monsoon"}
        };
        spell_names_data["Jarvan IV"] = {
            {'P', "Martial Cadence"},
            {'Q', "Dragon Strike"},
            {'W', "Golden Aegis"},
            {'E', "Demacian Standard"},
            {'R', "Cataclysm"}
        };
        spell_names_data["Jax"] = {
            {'P', "Relentless Assault"},
            {'Q', "Leap Strike"},
            {'W', "Empower"},
            {'E', "Counter Strike"},
            {'R', "Grandmaster's Might"}
        };
        spell_names_data["Jayce"] = {
            {'P', "Hextech Capacitor"},
            {'Q', "To the Skies! / Shock Blast"},
            {'W', "Lightning Field / Hyper Charge"},
            {'E', "Thundering Blow / Acceleration Gate"},
            {'R', "Transform: Mercury Cannon / Mercury Hammer"}
        };
        spell_names_data["Jhin"] = {
            {'P', "Whisper"},
            {'Q', "Dancing Grenade"},
            {'W', "Deadly Flourish"},
            {'E', "Captive Audience"},
            {'R', "Curtain Call"}
        };
        spell_names_data["Jinx"] = {
            {'P', "Get Excited!"},
            {'Q', "Switcheroo!"},
            {'W', "Zap!"},
            {'E', "Flame Chompers!"},
            {'R', "Super Mega Death Rocket!"}
        };
        spell_names_data["Kai'Sa"] = {
            {'P', "Second Skin"},
            {'Q', "Icathian Rain"},
            {'W', "Void Seeker"},
            {'E', "Supercharge"},
            {'R', "Killer Instinct"}
        };
        spell_names_data["Kalista"] = {
            {'P', "Martial Poise"},
            {'Q', "Pierce"},
            {'W', "Sentinel"},
            {'E', "Rend"},
            {'R', "Fate's Call"}
        };
        spell_names_data["Karma"] = {
            {'P', "Gathering Fire"},
            {'Q', "Inner Flame"},
            {'W', "Focused Resolve / Renewal"},
            {'E', "Inspire / Defiance"},
            {'R', "Mantra"}
        };
        spell_names_data["Karthus"] = {
            {'P', "Death Defied"},
            {'Q', "Lay Waste"},
            {'W', "Wall of Pain"},
            {'E', "Defile"},
            {'R', "Requiem"}
        };
        spell_names_data["Kassadin"] = {
            {'P', "Void Stone"},
            {'Q', "Null Sphere"},
            {'W', "Nether Blade"},
            {'E', "Force Pulse"},
            {'R', "Riftwalk"}
        };
        spell_names_data["Katarina"] = {
            {'P', "Voracity"},
            {'Q', "Bouncing Blade"},
            {'W', "Preparation"},
            {'E', "Shunpo"},
            {'R', "Death Lotus"}
        };
        spell_names_data["Kayle"] = {
            {'P', "Divine Ascent"},
            {'Q', "Radiant Blast"},
            {'W', "Celestial Blessing"},
            {'E', "Starfire Spellblade"},
            {'R', "Divine Judgment"}
        };
        spell_names_data["Kayn"] = {
            {'P', "The Darkin Scythe"},
            {'Q', "Reaping Slash"},
            {'W', "Blade's Reach"},
            {'E', "Shadow Step"},
            {'R', "Umbral Trespass"}
        };
        spell_names_data["Kennen"] = {
            {'P', "Mark of the Storm"},
            {'Q', "Thundering Shuriken"},
            {'W', "Electrical Surge"},
            {'E', "Lightning Rush"},
            {'R', "Slicing Maelstrom"}
        };
        spell_names_data["Kha'Zix"] = {
            {'P', "Unseen Threat"},
            {'Q', "Taste Their Fear"},
            {'W', "Void Spike"},
            {'E', "Leap"},
            {'R', "Void Assault"}
        };
        spell_names_data["Kindred"] = {
            {'P', "Mark of the Kindred"},
            {'Q', "Dance of Arrows"},
            {'W', "Wolf's Frenzy"},
            {'E', "Mounting Dread"},
            {'R', "Lamb's Respite"}
        };
        spell_names_data["Kled"] = {
            {'P', "Skaarl the Cowardly Lizard"},
            {'Q', "Beartrap on a Rope / Pocket Pistol"},
            {'W', "Violent Tendencies"},
            {'E', "Jousting"},
            {'R', "Chaaaaaaaarge!!!"}
        };
        spell_names_data["Kog'Maw"] = {
            {'P', "Icathian Surprise"},
            {'Q', "Caustic Spittle"},
            {'W', "Bio-Arcane Barrage"},
            {'E', "Void Ooze"},
            {'R', "Living Artillery"}
        };
        spell_names_data["K'Sante"] = {
            {'P', "Dauntless Instinct"},
            {'Q', "Ntofo Strikes"},
            {'W', "Path Maker"},
            {'E', "Footwork"},
            {'R', "All Out"}
        };
        spell_names_data["LeBlanc"] = {
            {'P', "Mirror Image"},
            {'Q', "Sigil of Malice"},
            {'W', "Distortion"},
            {'E', "Ethereal Chains"},
            {'R', "Mimic"}
        };
        spell_names_data["Lee Sin"] = {
            {'P', "Flurry"},
            {'Q', "Sonic Wave / Resonating Strike"},
            {'W', "Safeguard / Iron Will"},
            {'E', "Tempest / Cripple"},
            {'R', "Dragon's Rage"}
        };
        spell_names_data["Leona"] = {
            {'P', "Sunlight"},
            {'Q', "Shield of Daybreak"},
            {'W', "Eclipse"},
            {'E', "Zenith Blade"},
            {'R', "Solar Flare"}
        };
        spell_names_data["Lillia"] = {
            {'P', "Dream-Laden Bough"},
            {'Q', "Blooming Blows"},
            {'W', "Watch Out! Eep!"},
            {'E', "Swirlseed"},
            {'R', "Lilting Lullaby"}
        };
        spell_names_data["Lissandra"] = {
            {'P', "Iceborn Subjugation"},
            {'Q', "Ice Shard"},
            {'W', "Ring of Frost"},
            {'E', "Glacial Path"},
            {'R', "Frozen Tomb"}
        };
        spell_names_data["Lucian"] = {
            {'P', "Lightslinger"},
            {'Q', "Piercing Light"},
            {'W', "Ardent Blaze"},
            {'E', "Relentless Pursuit"},
            {'R', "The Culling"}
        };
        spell_names_data["Lulu"] = {
            {'P', "Pix, Faerie Companion"},
            {'Q', "Glitterlance"},
            {'W', "Whimsy"},
            {'E', "Help, Pix!"},
            {'R', "Wild Growth"}
        };
        spell_names_data["Lux"] = {
            {'P', "Illumination"},
            {'Q', "Light Binding"},
            {'W', "Prismatic Barrier"},
            {'E', "Lucent Singularity"},
            {'R', "Final Spark"}
        };
        spell_names_data["Malphite"] = {
            {'P', "Granite Shield"},
            {'Q', "Seismic Shard"},
            {'W', "Thunderclap"},
            {'E', "Ground Slam"},
            {'R', "Unstoppable Force"}
        };
        spell_names_data["Malzahar"] = {
            {'P', "Void Shift"},
            {'Q', "Call of the Void"},
            {'W', "Void Swarm"},
            {'E', "Malefic Visions"},
            {'R', "Nether Grasp"}
        };
        spell_names_data["Maokai"] = {
            {'P', "Sap Magic"},
            {'Q', "Bramble Smash"},
            {'W', "Twisted Advance"},
            {'E', "Sapling Toss"},
            {'R', "Nature's Grasp"}
        };
        spell_names_data["Master Yi"] = {
            {'P', "Double Strike"},
            {'Q', "Alpha Strike"},
            {'W', "Meditate"},
            {'E', "Wuju Style"},
            {'R', "Highlander"}
        };
        spell_names_data["Milio"] = {
            {'P', "Fired Up!"},
            {'Q', "Ultra Mega Fire Kick"},
            {'W', "Cozy Campfire"},
            {'E', "Warm Hugs"},
            {'R', "Breath of Life"}
        };
        spell_names_data["Miss Fortune"] = {
            {'P', "Love Tap"},
            {'Q', "Double Up"},
            {'W', "Strut"},
            {'E', "Make It Rain"},
            {'R', "Bullet Time"}
        };
        spell_names_data["Mordekaiser"] = {
            {'P', "Darkness Rise"},
            {'Q', "Obliterate"},
            {'W', "Indestructible"},
            {'E', "Death's Grasp"},
            {'R', "Realm of Death"}
        };
        spell_names_data["Morgana"] = {
            {'P', "Soul Siphon"},
            {'Q', "Dark Binding"},
            {'W', "Tormented Shadow"},
            {'E', "Black Shield"},
            {'R', "Soul Shackles"}
        };
        spell_names_data["Nami"] = {
           {'P', "Surging Tides"},
           {'Q', "Aqua Prison"},
           {'W', "Ebb and Flow"},
           {'E', "Tidecaller's Blessing"},
           {'R', "Tidal Wave"}
        };
        spell_names_data["Nasus"] = {
            {'P', "Soul Eater"},
            {'Q', "Siphoning Strike"},
            {'W', "Wither"},
            {'E', "Spirit Fire"},
            {'R', "Fury of the Sands"}
        };
        spell_names_data["Nautilus"] = {
            {'P', "Staggering Blow"},
            {'Q', "Dredge Line"},
            {'W', "Titan's Wrath"},
            {'E', "Riptide"},
            {'R', "Depth Charge"}
        };
        spell_names_data["Neeko"] = {
            {'P', "Inherent Glamour"},
            {'Q', "Blooming Burst"},
            {'W', "Shapesplitter"},
            {'E', "Tangle-Barbs"},
            {'R', "Pop Blossom"}
        };
        spell_names_data["Nidalee"] = {
            {'P', "Prowl"},
            {'Q', "Javelin Toss / Takedown"},
            {'W', "Bushwhack / Pounce"},
            {'E', "Primal Surge / Swipe"},
            {'R', "Aspect of the Cougar"}
        };
        spell_names_data["Nilah"] = {
            {'P', "Joy Unending"},
            {'Q', "Formless Blade"},
            {'W', "Jubilant Veil"},
            {'E', "Slipstream"},
            {'R', "Apotheosis"}
        };
        spell_names_data["Nocturne"] = {
            {'P', "Umbra Blades"},
            {'Q', "Duskbringer"},
            {'W', "Shroud of Darkness"},
            {'E', "Unspeakable Horror"},
            {'R', "Paranoia"}
        };
        spell_names_data["Nunu & Willump"] = {
            {'P', "Call of the Freljord"},
            {'Q', "Consume"},
            {'W', "Biggest Snowball Ever!"},
            {'E', "Snowball Barrage"},
            {'R', "Absolute Zero"}
        };
        spell_names_data["Olaf"] = {
            {'P', "Berserker Rage"},
            {'Q', "Undertow"},
            {'W', "Tough It Out"},
            {'E', "Reckless Swing"},
            {'R', "Ragnarok"}
        };
        spell_names_data["Orianna"] = {
            {'P', "Clockwork Windup"},
            {'Q', "Command: Attack"},
            {'W', "Command: Dissonance"},
            {'E', "Command: Protect"},
            {'R', "Command: Shockwave"}
        };
        spell_names_data["Ornn"] = {
            {'P', "Living Forge"},
            {'Q', "Volcanic Rupture"},
            {'W', "Bellows Breath"},
            {'E', "Searing Charge"},
            {'R', "Call of the Forge God"}
        };
        spell_names_data["Pantheon"] = {
            {'P', "Mortal Will"},
            {'Q', "Comet Spear"},
            {'W', "Shield Vault"},
            {'E', "Aegis Assault"},
            {'R', "Grand Starfall"}
        };
        spell_names_data["Poppy"] = {
            {'P', "Iron Ambassador"},
            {'Q', "Hammer Shock"},
            {'W', "Steadfast Presence"},
            {'E', "Heroic Charge"},
            {'R', "Keeper's Verdict"}
        };
        spell_names_data["Pyke"] = {
            {'P', "Gift of the Drowned Ones"},
            {'Q', "Bone Skewer"},
            {'W', "Ghostwater Dive"},
            {'E', "Phantom Undertow"},
            {'R', "Death From Below"}
        };
        spell_names_data["Qiyana"] = {
            {'P', "Royal Privilege"},
            {'Q', "Edge of Ixtal / Elemental Wrath"},
            {'W', "Terrashape"},
            {'E', "Audacity"},
            {'R', "Supreme Display of Talent"}
        };
        spell_names_data["Quinn"] = {
            {'P', "Harrier"},
            {'Q', "Blinding Assault"},
            {'W', "Heightened Senses"},
            {'E', "Vault"},
            {'R', "Behind Enemy Lines"}
        };
        spell_names_data["Rakan"] = {
            {'P', "Fey Feathers"},
            {'Q', "Gleaming Quill"},
            {'W', "Grand Entrance"},
            {'E', "Battle Dance"},
            {'R', "The Quickness"}
        };
        spell_names_data["Rammus"] = {
            {'P', "Rolling Armordillo"},
            {'Q', "Powerball"},
            {'W', "Defensive Ball Curl"},
            {'E', "Frenzying Taunt"},
            {'R', "Soaring Slam"}
        };
        spell_names_data["Rek'Sai"] = {
            {'P', "Fury of the Xer'Sai"},
            {'Q', "Queen's Wrath / Prey Seeker"},
            {'W', "Burrow / Un-burrow"},
            {'E', "Furious Bite / Tunnel"},
            {'R', "Void Rush"}
        };
        spell_names_data["Rell"] = {
           {'P', "Break the Mold"},
           {'Q', "Shattering Strike"},
           {'W', "Ferromancy: Crash Down / Mount Up"},
           {'E', "Attract and Repel"},
           {'R', "Magnet Storm"}
        };
        spell_names_data["Renata Glasc"] = {
            {'P', "Leverage"},
            {'Q', "Handshake"},
            {'W', "Bailout"},
            {'E', "Loyalty Program"},
            {'R', "Hostile Takeover"}
        };
        spell_names_data["Renekton"] = {
            {'P', "Reign of Anger"},
            {'Q', "Cull the Meek"},
                    {'W', "Ruthless Predator"},
        {'E', "Slice and Dice"},
        {'R', "Dominus"}
        };
        spell_names_data["Rengar"] = {
            {'P', "Unseen Predator"},
            {'Q', "Savagery"},
            {'W', "Battle Roar"},
            {'E', "Bola Strike"},
            {'R', "Thrill of the Hunt"}
        };
        spell_names_data["Riven"] = {
            {'P', "Runic Blade"},
            {'Q', "Broken Wings"},
            {'W', "Ki Burst"},
            {'E', "Valor"},
            {'R', "Blade of the Exile"}
        };
        spell_names_data["Rumble"] = {
            {'P', "Junkyard Titan"},
            {'Q', "Flamespitter"},
            {'W', "Scrap Shield"},
            {'E', "Electro Harpoon"},
            {'R', "The Equalizer"}
        };
        spell_names_data["Ryze"] = {
            {'P', "Arcane Mastery"},
            {'Q', "Overload"},
            {'W', "Rune Prison"},
            {'E', "Spell Flux"},
            {'R', "Realm Warp"}
        };
        spell_names_data["Samira"] = {
            {'P', "Daredevil Impulse"},
            {'Q', "Flair"},
            {'W', "Blade Whirl"},
            {'E', "Wild Rush"},
            {'R', "Inferno Trigger"}
        };
        spell_names_data["Sejuani"] = {
            {'P', "Fury of the North"},
            {'Q', "Arctic Assault"},
            {'W', "Winter's Wrath"},
            {'E', "Permafrost"},
            {'R', "Glacial Prison"}
        };
        spell_names_data["Senna"] = {
            {'P', "Absolution"},
            {'Q', "Piercing Darkness"},
            {'W', "Last Embrace"},
            {'E', "Curse of the Black Mist"},
            {'R', "Dawning Shadow"}
        };
        spell_names_data["Seraphine"] = {
            {'P', "Stage Presence"},
            {'Q', "High Note"},
            {'W', "Surround Sound"},
            {'E', "Beat Drop"},
            {'R', "Encore"}
        };
        spell_names_data["Sett"] = {
            {'P', "Pit Grit"},
            {'Q', "Knuckle Down"},
            {'W', "Haymaker"},
            {'E', "Facebreaker"},
            {'R', "The Show Stopper"}
        };
        spell_names_data["Shaco"] = {
            {'P', "Backstab"},
            {'Q', "Deceive"},
            {'W', "Jack In The Box"},
            {'E', "Two-Shiv Poison"},
            {'R', "Hallucinate"}
        };
        spell_names_data["Shen"] = {
            {'P', "Ki Barrier"},
            {'Q', "Twilight Assault"},
            {'W', "Spirit's Refuge"},
            {'E', "Shadow Dash"},
            {'R', "Stand United"}
        };
        spell_names_data["Shyvana"] = {
            {'P', "Fury of the Dragonborn"},
            {'Q', "Twin Bite"},
            {'W', "Burnout"},
            {'E', "Flame Breath"},
            {'R', "Dragon's Descent"}
        };
        spell_names_data["Singed"] = {
            {'P', "Noxious Slipstream"},
            {'Q', "Poison Trail"},
            {'W', "Mega Adhesive"},
            {'E', "Fling"},
            {'R', "Insanity Potion"}
        };
        spell_names_data["Sion"] = {
            {'P', "Glory in Death"},
            {'Q', "Decimating Smash"},
            {'W', "Soul Furnace"},
            {'E', "Roar of the Slayer"},
            {'R', "Unstoppable Onslaught"}
        };
        spell_names_data["Sivir"] = {
            {'P', "Fleet of Foot"},
            {'Q', "Boomerang Blade"},
            {'W', "Ricochet"},
            {'E', "Spell Shield"},
            {'R', "On The Hunt"}
        };
        spell_names_data["Skarner"] = {
            {'P', "Crystal Spires"},
            {'Q', "Crystal Slash / Fracture"},
            {'W', "Crystalline Exoskeleton"},
            {'E', "Fracture"},
            {'R', "Impale"}
        };
        spell_names_data["Smolder"] = {
            {'P', "Dragon Practice"},
            {'Q', "Super Scorcher Breath"},
            {'W', "Achooo!"},
            {'E', "Flap, Flap, Flap"},
            {'R', "MMOOOMMM!"}
        };
        spell_names_data["Sona"] = {
            {'P', "Power Chord"},
            {'Q', "Hymn of Valor"},
            {'W', "Aria of Perseverance"},
            {'E', "Song of Celerity"},
            {'R', "Crescendo"}
        };
        spell_names_data["Soraka"] = {
            {'P', "Salvation"},
            {'Q', "Starcall"},
            {'W', "Astral Infusion"},
            {'E', "Equinox"},
            {'R', "Wish"}
        };
        spell_names_data["Swain"] = {
            {'P', "Ravenous Flock"},
            {'Q', "Death's Hand"},
            {'W', "Vision of Empire"},
            {'E', "Nevermove"},
            {'R', "Demonic Ascension / Demonflare"}
        };
        spell_names_data["Sylas"] = {
            {'P', "Petricite Burst"},
            {'Q', "Chain Lash"},
            {'W', "Kingslayer"},
            {'E', "Abscond / Abduct"},
            {'R', "Hijack"}
        };
        spell_names_data["Syndra"] = {
            {'P', "Transcendent"},
            {'Q', "Dark Sphere"},
            {'W', "Force of Will"},
            {'E', "Scatter the Weak"},
            {'R', "Unleashed Power"}
        };
        spell_names_data["Tahm Kench"] = {
            {'P', "An Acquired Taste"},
            {'Q', "Tongue Lash"},
            {'W', "Abyssal Dive"},
            {'E', "Thick Skin"},
            {'R', "Devour"}
        };
        spell_names_data["Taliyah"] = {
            {'P', "Rock Surfing"},
            {'Q', "Threaded Volley"},
            {'W', "Seismic Shove"},
            {'E', "Unraveled Earth"},
            {'R', "Weaver's Wall"}
        };
        spell_names_data["Talon"] = {
            {'P', "Blade's End"},
            {'Q', "Noxian Diplomacy"},
            {'W', "Rake"},
            {'E', "Assassin's Path"},
            {'R', "Shadow Assault"}
        };
        spell_names_data["Taric"] = {
            {'P', "Bravado"},
            {'Q', "Starlight's Touch"},
            {'W', "Bastion"},
            {'E', "Dazzle"},
            {'R', "Cosmic Radiance"}
        };
        spell_names_data["Teemo"] = {
            {'P', "Guerrilla Warfare"},
            {'Q', "Blinding Dart"},
            {'W', "Move Quick"},
            {'E', "Toxic Shot"},
            {'R', "Noxious Trap"}
        };
        spell_names_data["Thresh"] = {
            {'P', "Damnation"},
            {'Q', "Death Sentence"},
            {'W', "Dark Passage"},
            {'E', "Flay"},
            {'R', "The Box"}
        };
        spell_names_data["Tristana"] = {
            {'P', "Draw a Bead"},
            {'Q', "Rapid Fire"},
            {'W', "Rocket Jump"},
            {'E', "Explosive Charge"},
            {'R', "Buster Shot"}
        };
        spell_names_data["Trundle"] = {
            {'P', "King's Tribute"},
            {'Q', "Chomp"},
            {'W', "Frozen Domain"},
            {'E', "Pillar of Ice"},
            {'R', "Subjugate"}
        };
        spell_names_data["Tryndamere"] = {
            {'P', "Battle Fury"},
            {'Q', "Bloodlust"},
            {'W', "Mocking Shout"},
            {'E', "Spinning Slash"},
            {'R', "Undying Rage"}
        };
        spell_names_data["Twisted Fate"] = {
            {'P', "Loaded Dice"},
            {'Q', "Wild Cards"},
            {'W', "Pick a Card"},
            {'E', "Stacked Deck"},
            {'R', "Destiny"}
        };
        spell_names_data["Twitch"] = {
            {'P', "Deadly Venom"},
            {'Q', "Ambush"},
            {'W', "Venom Cask"},
            {'E', "Contaminate"},
            {'R', "Spray and Pray"}
        };
        spell_names_data["Udyr"] = {
            {'P', "Bridge Between"},
            {'Q', "Wilding Claw"},
            {'W', "Iron Mantle"},
            {'E', "Blazing Stampede"},
            {'R', "Wingborne Storm"}
        };
        spell_names_data["Urgot"] = {
            {'P', "Echoing Flames"},
            {'Q', "Corrosive Charge"},
            {'W', "Purge"},
            {'E', "Disdain"},
            {'R', "Fear Beyond Death"}
        };
        spell_names_data["Varus"] = {
            {'P', "Living Vengeance"},
            {'Q', "Piercing Arrow"},
            {'W', "Blighted Quiver"},
            {'E', "Hail of Arrows"},
            {'R', "Chain of Corruption"}
        };
        spell_names_data["Vayne"] = {
            {'P', "Night Hunter"},
            {'Q', "Tumble"},
            {'W', "Silver Bolts"},
            {'E', "Condemn"},
            {'R', "Final Hour"}
        };
        spell_names_data["Veigar"] = {
            {'P', "Phenomenal Evil Power"},
            {'Q', "Baleful Strike"},
            {'W', "Dark Matter"},
            {'E', "Event Horizon"},
            {'R', "Primordial Burst"}
        };
        spell_names_data["Vel'Koz"] = {
            {'P', "Organic Deconstruction"},
            {'Q', "Plasma Fission"},
            {'W', "Void Rift"},
            {'E', "Tectonic Disruption"},
            {'R', "Life Form Disintegration Ray"}
        };
        spell_names_data["Vex"] = {
            {'P', "Doom 'n Gloom"},
            {'Q', "Mistral Bolt"},
            {'W', "Personal Space"},
            {'E', "Looming Darkness"},
            {'R', "Shadow Surge"}
        };
        spell_names_data["Vi"] = {
            {'P', "Blast Shield"},
            {'Q', "Vault Breaker"},
            {'W', "Denting Blows"},
            {'E', "Relentless Force"},
            {'R', "Cease and Desist"}
        };
        spell_names_data["Viego"] = {
            {'P', "Sovereign's Domination"},
            {'Q', "Blade of the Ruined King"},
            {'W', "Spectral Maw"},
            {'E', "Harrowed Path"},
            {'R', "Heartbreaker"}
        };
        spell_names_data["Viktor"] = {
            {'P', "Glorious Evolution"},
            {'Q', "Siphon Power"},
            {'W', "Gravity Field"},
            {'E', "Death Ray"},
            {'R', "Chaos Storm"}
        };
        spell_names_data["Vladimir"] = {
            {'P', "Crimson Pact"},
            {'Q', "Transfusion"},
            {'W', "Sanguine Pool"},
            {'E', "Tides of Blood"},
            {'R', "Hemoplague"}
        };
        spell_names_data["Volibear"] = {
            {'P', "The Relentless Storm"},
            {'Q', "Thundering Smash"},
            {'W', "Frenzied Maul"},
            {'E', "Sky Splitter"},
            {'R', "Stormbringer"}
        };
        spell_names_data["Warwick"] = {
            {'P', "Eternal Hunger"},
            {'Q', "Jaws of the Beast"},
            {'W', "Blood Hunt"},
            {'E', "Primal Howl"},
            {'R', "Infinite Duress"}
        };
        spell_names_data["Wukong"] = {
            {'P', "Stone Skin"},
            {'Q', "Crushing Blow"},
            {'W', "Warrior Trickster"},
            {'E', "Nimbus Strike"},
            {'R', "Cyclone"}
        };
        spell_names_data["Xayah"] = {
            {'P', "Clean Cuts"},
            {'Q', "Double Daggers"},
            {'W', "Deadly Plumage"},
            {'E', "Bladecaller"},
            {'R', "Featherstorm"}
        };
        spell_names_data["Xerath"] = {
            {'P', "Mana Surge"},
            {'Q', "Arcanopulse"},
            {'W', "Eye of Destruction"},
            {'E', "Shocking Orb"},
            {'R', "Rite of the Arcane"}
        };
        spell_names_data["Xin Zhao"] = {
            {'P', "Determination"},
            {'Q', "Three Talon Strike"},
            {'W', "Wind Becomes Lightning"},
            {'E', "Audacious Charge"},
            {'R', "Crescent Guard"}
        };
        spell_names_data["Yasuo"] = {
            {'P', "Way of the Wanderer"},
            {'Q', "Steel Tempest"},
            {'W', "Wind Wall"},
            {'E', "Sweeping Blade"},
            {'R', "Last Breath"}
        };
        spell_names_data["Yone"] = {
            {'P', "Way of the Hunter"},
            {'Q', "Mortal Steel"},
            {'W', "Spirit Cleave"},
            {'E', "Soul Unbound"},
            {'R', "Fate Sealed"}
        };
        spell_names_data["Yorick"] = {
            {'P', "Shepherd of Souls"},
            {'Q', "Last Rites / Awakening"},
            {'W', "Dark Procession"},
            {'E', "Mourning Mist"},
            {'R', "Eulogy of the Isles"}
        };
        spell_names_data["Yuumi"] = {
            {'P', "Bop 'n' Block"},
            {'Q', "Prowling Projectile"},
            {'W', "You and Me!"},
            {'E', "Zoomies"},
            {'R', "Final Chapter"}
        };
        spell_names_data["Zac"] = {
            {'P', "Cell Division"},
            {'Q', "Stretching Strikes"},
            {'W', "Unstable Matter"},
            {'E', "Elastic Slingshot"},
            {'R', "Let's Bounce!"}
        };
        spell_names_data["Zed"] = {
            {'P', "Contempt for the Weak"},
            {'Q', "Razor Shuriken"},
            {'W', "Living Shadow"},
            {'E', "Shadow Slash"},
            {'R', "Death Mark"}
        };
        spell_names_data["Zeri"] = {
            {'P', "Living Battery"},
            {'Q', "Burst Fire"},
            {'W', "Ultrashock Laser"},
            {'E', "Spark Surge"},
            {'R', "Lightning Crash"}
        };
        spell_names_data["Ziggs"] = {
            {'P', "Short Fuse"},
            {'Q', "Bouncing Bomb"},
            {'W', "Satchel Charge"},
            {'E', "Hexplosive Minefield"},
            {'R', "Mega Inferno Bomb"}
        };
        spell_names_data["Zilean"] = {
            {'P', "Time in a Bottle"},
            {'Q', "Time Bomb"},
            {'W', "Rewind"},
            {'E', "Time Warp"},
            {'R', "Chronoshift"}
        };
        spell_names_data["Zoe"] = {
            {'P', "More Sparkles!"},
            {'Q', "Paddle Star!"},
            {'W', "Spell Thief"},
            {'E', "Sleepy Trouble Bubble"},
            {'R', "Portal Jump"}
        };
        spell_names_data["Zyra"] = {
            {'P', "Garden of Thorns"},
            {'Q', "Deadly Spines"},
            {'W', "Rampant Growth"},
            {'E', "Grasping Roots"},
            {'R', "Stranglethorns"}
        };
    }


    std::string get_champion_spell_name(const std::string& champion_name, char spell_slot) {
        initialize_spell_names();

        std::string lower_champion_name = champion_name;
        std::transform(lower_champion_name.begin(), lower_champion_name.end(), lower_champion_name.begin(), ::tolower);

        for (const auto& pair : spell_names_data) {
            std::string lower_key = pair.first;
            std::transform(lower_key.begin(), lower_key.end(), lower_key.begin(), ::tolower);

            if (lower_key == lower_champion_name) {
                auto spell_it = pair.second.find(spell_slot);
                if (spell_it != pair.second.end()) {
                    return spell_it->second;
                }
            }
        }
        return "Unknown Spell";
    }

    std::string get_champion_q_name(const std::string& champion_name) {
        return get_champion_spell_name(champion_name, 'Q');
    }

    std::string get_champion_w_name(const std::string& champion_name) {
        return get_champion_spell_name(champion_name, 'W');
    }

    std::string get_champion_e_name(const std::string& champion_name) {
        return get_champion_spell_name(champion_name, 'E');
    }

    std::string get_champion_r_name(const std::string& champion_name) {
        return get_champion_spell_name(champion_name, 'R');
    }
    std::string get_champion_p_name(const std::string& champion_name) {
        return get_champion_spell_name(champion_name, 'P');
    }

    std::map<std::string, std::map<char, std::string>> champion_spell_names::spell_names_data;

}
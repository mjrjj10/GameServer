# MxM masters – skill mechanics from external guides

Source: Inven Global in-depth master guides (2017, MXM live service). Used to check server behaviour
against the original game. Mechanics only (no build advice). Numbers are those of the 2017 game.

## V-Merang (BOOMERANG) — invenglobal.com/articles/2243
- Passive Sametan Warrior: stores 2 Ringblades, 1 consumed per skill. Weapon attack: bonus damage every 8 s, binds target 1 s.
- Arcblade (A): Ringblade along a circular path that returns to her, "bends back to V-Merang, even if she moves after using this skill". Multi-target, long range.
- Backspin Blade (E): Ringblade at a set area, struck target becomes airborne. Very short range.
  In-game KR description: travels a fixed distance, then rises into the air and launches enemies (gameplay video confirms it pierces).
- Scatterblade: targets the enemy near the cursor hitbox, homing ("hits your target even if it moves"), bounces to another nearby target on hit.
- Ringblade Barrier: spinning Ringblade at a fixed area for a duration, damages and reflects enemy projectiles only.
- Ultimate Blade Storm: tornado in a straight line, 200 m (20000 units?), wide, fast, launches struck targets; damage -40% until the tornado is 8 m from V-Merang.
- Acrobatics (dodge): 8 m toward the cursor, CC immune while moving.

## Lua (LAUNCHER) — invenglobal.com/articles/2475
- Passive Firearms Proficiency: a free (costless) skill after using 5 skills.
- Shrapnel Rocket: straight rocket, "explodes on the first target it hits or when it reaches the end of its range", cone damage after explosion.
- Claymore Charge: dashes to a target and pushes it, then a missile knocks it further back.
- Molten Rain: missile at a target area, 4 explosions, 1 s delay before activation.
- Distortion Shell: jumps 10 m, 6 m zone at the launch point restricting enemy weapon attacks.
- Ultimate Napalm Wave: cone of successive explosions from the cast point, knocks enemies airborne.
- Swift Roll: 5 m roll, large stamina cost.

## Taejin (STRIKER) — invenglobal.com/articles/2496
- Passive Combat Instinct: recovers a little stamina on each weapon hit.
- Concussion Blast: close-range small cone, knockback, quick.
- Proximity Grenade: thrown to an area, damage on hit and on explosion, detonates after 3 s if no target, goes off when stepped on.
- Explosive Barrage: line of explosive shells, explodes 4 times, slow projectile.
- Inspiring Presence: +20% weapon damage and move speed for him and allies within 10 m, persists through tagging.
- Ultimate Dredgion Strike: missiles on an area, slow 40% for 2 s, takes time to fire.
- Roll: short roll, CC immune, lots of stamina, 0.3 s cooldown.

## Kromede — invenglobal.com/articles/2497
- Passive Bleed: attacking stunned/bound/shocked enemies makes them bleed 5 s; bleeding enemies take +10% from her skills; her damage -30% within 5 m.
- Galeforce Arrow: single long-range arrow, high damage, knockback; stun 1 s if the target collides with another enemy.
- Aetherpike Shards: line of spikes; stepping on one binds 4.5 s and destroys it; spikes block attacks.
- Quick Draw: up to 3 consecutive casts: 1, 3, then 5 arrows in a fan.
- Blot Out the Sun: arrows on an area, damage over time, +10% on bleeding.
- Ultimate Righteous Justice: 7 s, auto attacks fire 5 arrows in a fan, less heat buildup.
- Windborne Shield: blocks one attack, 4 s, +50% move speed.

## Lilu — invenglobal.com/articles/2467
- Passive Razor Beak: every 4th attack deals extra damage.
- Bird of Prey: hawk lifts an enemy (or ally) and drops it at a chosen spot; airborne target can't attack/dodge/cast.
- Talon Swarm: AoE with damage over time, reveals area and stealth, faster neutral kills.
- Birds of a Feather: mobility, CC immune.
- Feather Crest: line strike marking the first target (revealed 3 s); the eagle flies to it, stunning the target and enemies on the way.
- Wingrider (survival): lifted into the air, dodges attacks.
- Ultimate Screeching Crest: mark dealing damage over time to the target and around it; removed if the target tags out.

## Rytlock — invenglobal.com/articles/2466
- Passive: 1st and 2nd weapon attacks burn; hitting a burning target +20% damage.
- Tenacious (survival): immune to the next CC after being struck by CC.
- Brutish Leap: leap to an area, 280 dmg, slow 25% 1.5 s, cd 16 s, range 10 m.
- Hamstring: damage after a short delay, 700 dmg, slow 30% 2 s, cd 12 s, short range.
- Head Shot: ranged 190 dmg, 12 m, short silence.
- Pistol Whip: swipe stun 1.5 s then 5 strikes (270 total), cd 18 s, any action cancels it.
- Ultimate Sohothin's Wrath: weapon attacks become long-range piercing for 10 s or 6 attacks, burn 4 s.

## Ghost Widow — invenglobal.com/articles/2453
- Passive Consuming Darkness: +12% weapon damage per debuff on the target.
- Life Drain: tether 7.5 s, damage every 1.5 s, heals her, breaks if too far; she can move and attack.
- Fearsome Stare: projectile hits the FIRST target in line (no pierce), fear 1.8 s, explodes on impact or max range.
- Tenebrous Tentacles: DoT 5 s on the target, others in a small circle take 50% of it.
- Ethereal Images: ghost form 3 s, 3 copies roam then explode, immune 3 s, can't attack/cast/jump.
- Ultimate Soul Storm: target lifted 3.5 s, 3 blasts, each hit spawns 2 missiles; channel (moving/attacking cancels).
- Dark Passage: 3 m dash stripping buffs from enemies hit.

## Kat the Cat — invenglobal.com/articles/2432
- Passive: +8% attack speed for 5 s per skill used.
- Mew: stuns around 0.5 s; stance: long-range weapon attacks for 15 s or 7 attacks.
- Kai: reflects 50% damage for 0.8 s; stance: short swipes, +10% move speed and crit, swipes refresh the reflect.
- King: binds around 1 s; stance: splash attacks, slow 15% 1.2 s (2 stacks), +15% defense, 10 HP regen.
- Queen: stance: mid-range fans, allies/enemies hit get +10% damage and +10% defense 10 s.
- Ultimate Pounce: jumps onto an ally/enemy (2 charges), knocks back enemies on the way, knocks down the target;
  ally: -1 debuff, +10% ms 3 s, 100 HP over 3 s; enemy: dispel 1 buff, -10% weapon dmg 3 s, Kat +10% weapon dmg.
- If I Fits, I Sits: invincible 1.2 s, can't move, then +50% move speed briefly.

## Vita — invenglobal.com/articles/2430
- Passive Harmony: every 3rd skill a shield for 6 s (absorbs damage, regenerates MP); on expiry -30% skill cooldowns.
- Wooly Ballad: click-to-hit polymorph into a lamb, slowed 30%.
- Note of Nullification: ally: shield blocking 400 damage; enemy: removes buffs, damage per buff removed.
- Valiabolt: long-range projectile PIERCING several targets; allies hit healed 250, enemies damaged + slowed 50%.
- Vital Aura: hearts orbiting close around her, move speed buff; allies touching healed 100, enemies damaged + silenced.
- Dizzying Twirl: evades attacks with a quick turn.
- Ultimate Aria of Life: within 5 m, cancels enemy attacks, nobody can attack or move, all immune; allies recover HP/stamina.

## Merope — invenglobal.com/articles/2428
- Missile Salvo: click-drag to set start point and direction of a missile line, 12 m, hits behind walls, 795 dmg.
- Devastator Rocket: slow missile damaging along its path, explodes at the target area.
- Vector Strike: accelerates in a straight line, STOPS on the first enemy hit, 715 dmg, knockback; also a dash.
- Conversion Battery: 200 MP, energy field 5 s nullifying up to 5 attacks; after 5: +200 MP and 10% max HP.
- Ultimate Target Locked: homing missiles in a fan, up to 4 targets (fewer targets = several missiles each).
- Boost (survival): 7 m. Passive: weapon heat reduced after a skill cast; +10% weapon damage with another android tagged.

## Statesman — invenglobal.com/articles/2426
- Heroism: flies 8 s, +5% ms, +10% defense, ranged weapon attacks, empowered skills; resets non-ult cooldowns, +20% MP.
- Ultimate Olympian Bolts: 4 lightning bolts, base 50 dmg, slow 30% 4 s, cd 9 s.
- Flying Haymaker: flies to the target location and strikes; airborne during Heroism; cd 14 s.
- Foot Stomp: shockwave knockback, 285 dmg; Haymaker + Stomp binds.
- Dull Pain: heal 10% max HP, removes all debuffs, +2% per debuff; under Heroism also allies within 6 m.
- Unstoppable (survival): immune to damage and CC 2 s, then +15% ms 2 s. Passive: CC duration -25%.

## Jingtai — invenglobal.com/articles/2423
- Passive: stamina after every 4 skills; +1% weapon damage per strike (max 10%); last strike of the string launches airborne.
- Million Staff Burst: multi-hit short line, -10% target defense.
- Sweep Aside: pushes enemies in the charge direction; second press dashes to the pushed enemy.
- Bounding Strike: leap to area (hold = farther), damage on landing.
- Fling: dashes to an enemy and throws it over the shoulder (knockdown).
- Pogo (survival): invincible 1 s; reuse leaps forward.
- Ultimate Spirit of Protection: projection charging forward, damages along the path, blocks projectiles.

## Cagnazzo — invenglobal.com/articles/2408
- Passive Fever Pitch: gains RAGE when dealing OR receiving damage; full rage empowers the next skill and consumes the bar.
- Weapon: whirling swing hitting all enemies in range.
- Ripline: hook in a line, drags the FIRST struck opponent back, 690 dmg; lvl 3 drags several; empowered +30% dmg, +0.8 m width.
- Savage Leap: leap to an area, 70 dmg, stun 1 s, damage/stun decrease with distance from center, cd 12 s.
- Terrorize: 9 m radius, fear 2 s.
- Crushing Swing: knockdown, 315 dmg; lvl 6 binds 3.5 s.
- Barbaric Frenzy: 7 s CC immunity, more weapon/skill damage and defense.
- Charge (survival): charges forward ignoring CC.

## Innowin — invenglobal.com/articles/2402
- Weapon Nova: -30% damage within 5 m; charged (3-slot battery).
- Energy Lance: delayed (few s) line hitting all targets in it.
- Shot Caller: target can't stealth 5 s, more crit taken.
- Explosive Escape: explosion at her position launching her airborne; enemies 525 dmg + knockback; no damage taken while airborne.
- Paralyzer: straight projectile, immobilize 2 s; lvl 3 pierces.
- Ultimate Zeroing In: stealth + reticle following the cursor slowly, 1590 dmg, CC immune while stealthed, kill refunds ult.
- Passive: +33% crit after hitting an enemy. Survival: 3.6 m dash.

## Mondo Zax — invenglobal.com/articles/2397
- Prism Bot: cloaking field 1.5 s for everyone in the area; periodic +10% weapon dmg 5 s.
- Bruiser Bot: taunts nearby enemies 3 s; enemies within 15 m of the bot -30% ms; bot hits slow 1 s.
- Artillery Bot: 4 mortar shells on an area (320); the bot follows him and fires mortars periodically.
- Repair Bot: moves to an area, 250 shield to nearby allies 5 s; periodic 100 shield on him.
- Ultimate Orbital Laser: 680 dmg, 5 s cast time, slowly tracks the cursor.
- Passive: next attack within 5 s after a skill deals bonus damage. Survival: rockets away, untargetable.
- A new bot destroys the current one; enemies can destroy bots.

## Jinsoyun — invenglobal.com/articles/2377
- Passive Soul Blade: up to 5 stacks, +1 per 2 auto attacks or per dodged attack; 2+ stacks empower skills.
- Air Dash: dash behind the targeted enemy, usable twice; empowered slows. Combo: Rising Blade.
- Rising Blade: airborne on the target, no bush stealth 3 s, short range; empowered extra hit.
- Blade Rush: dash to a ground spot, damage on landing after a short delay; empowered: target can't move 1 s.
- Chi Pull: pulls the target to her, delayed; empowered + hit heals 15% max HP.
- Ultimate Shock Blade: triangle field at a spot, 5 s, only the lightning BEAMS shock (1 s); the center is safe.
- Guardian Force: invincible 0.5 s, deflects projectiles; on success +15% ms 2 s.

## Nedien — invenglobal.com/articles/2338
- Bottled Miasma (toggle): poison bottle every 1 s at her position while mana lasts; -20% defense 2 s; adds venom stacks.
- Toxin Toss: bottle at an area, -30% ms 1.5 s.
- Crippling Slingblades: 3 blades orbiting her; each recast flings one; slows 10/20/30%.
- Vitriolic Rush: charge to an area, enemies can't weapon-attack 1 s.
- Ultimate Blade Frenzy: 7 s attack/move speed; venom triggers add -20% weapon damage and skill power.
- Passive Creeping Venom: venom on skill use and every 3rd attack; at 3 stacks burst + 4% max HP heal.
- Skitter (survival): 6 m dash, CC immune.

## Koom — invenglobal.com/articles/2334
- Weapon does not overheat.
- Magnetic Barrier: frontal shield blocking projectiles 4 s (follows him, fixed facing), +20% ms 3 s; not AoE.
- Runeslab Barricade: stone wall at an area, blocks attacks and movement, minions attack it.
- Unstable Shell: shield; explodes when broken or expired, damage scales with absorbed damage; persists through tag.
- Crashing Defense: charges to an area, shields himself and allies in range.
- Ultimate Impervious Stone: undying 4 s; then +20% max HP plus more based on damage taken.
- Passive: gains Will (resource) when dealing or taking damage, decays over time; 3% HP; 1 s CC immunity on skill use.
- Granite Sheath (survival): 400 shield, +15% ms, +20 Will.

## R&B — invenglobal.com/articles/2331
- Weapon: targets an area, AoE, no overheat, interval between shots.
- Ram's Well: vacuum damaging and pulling enemies to the center, small delay, wide.
- Bacchus's Fist: small area, quick, big damage.
- Bacchus's Retaliation: 1 s window returning one melee attack, stuns the attacker 1 s.
- Ram's Impaler: line attack moving R&B behind the target (no dash on miss).
- Ultimate Duality Slam: 3.5 s cast, invincible, binds ALL enemies on the map 2 s, +20% HP and 300 MP.
- Vacuum Pulse: next weapon attack slows 30% 2 s, cd 8 s. Shift (survival): 7 m leap, weapon immunity 0.3 s.

## Maia — invenglobal.com/articles/2316
- Combat Protocol: shield 2 s, more range/damage/attack speed.
- Recall Tether: recall pad 5 s; allies stepping on it are pulled to her; she gains +50% ms.
- Savior Protocol: flies to an ally, picks it up (shield), dashes and drops it.
- Anti-Kinesis Barrier: enemies touching it are knocked back (once per 2 s per target).
- Passive: after a skill, 150 HP to the most wounded master within 8 m.
- Ultimate Synergy Protocol: teleports to the lowest-HP ally anywhere, invincible 2 s, brief delay. Survival: 7 m dash.

## Sonid — invenglobal.com/articles/2314
- Scatterball: 4 balls in a cone (6 at level 6), several can hit one target with reduced damage.
- Catcher's Mitt: dash blocking a projectile; recast throws a damaging ball.
- Power Slide: slide to an area, pushes the FIRST enemy hit back 2 m (knockdown at level 6).
- Firecracker: ball at the target leaving a fire line, damage for 3 s in it.
- Passive Underdog: +5% crit per 10% missing HP.
- Dustkicker (survival): slide + dust cloud nullifying all weapon attacks inside.
- Ultimate: launches the target (ally or enemy) and masters near it 12 m into the air.
- Weapon: charged, more damage and projectile speed.

## Ignuma — invenglobal.com/articles/2307
- Pyrobarrier: blocks the next attack; on block teleports forward with an explosion behind him.
- Set Ablaze (toggle): burning area following him, periodic low damage.
- Fiery Advance: charge; colliding with an enemy explodes, knocking back enemies around.
- Elemental Wrath: flaming orb at a target, fear 1.5 s, fire damage around.
- Ultimate Stoke the Coals: heal 20% HP + 100 per burning target in range, max 700.
- Passive Incinerate: skill hits burn 5 s (heal on hit), up to 4 stacks; the 4th deals extra damage and clears the burn.
- Molten Shield (survival): 4 s frontal projectile block, CC immune, +15% ms 2 s, can move, no skills/attacks, locked facing.

## Moro — invenglobal.com/articles/2247
- Robos are static, lose HP over time and when hit; deploying costs mana; more robos = more damage; dead robos drop a mana battery.
- Rocket Robo: attacks nearby enemies; Moro +100% projectile speed 5 s.
- Carbo-gel Robo: gel area disabling enemy weapon attacks; Moro +5% weapon dmg 5 s per enemy skill cast.
- Graviton Robo: LINE area slowing enemies passing through; Moro's attacks slow 30% for 5 s.
- Medi-Robo: heals allies in a small radius; heals Moro instantly.
- Ultimate Mecha-Moro: exo-suit, homing missiles every 1 s, healing/graviton aura, interrupts enemies; Moro slowed.
- Weapon: slow charged projectile healing robos (splash) or restoring ally stamina. Panic (survival): ~9 m.

## Poharan — invenglobal.com/articles/2245
- Blitz Grenade: explodes on landing, then a small burning area (DoT).
- Gatling Jump: jumps forward, slight damage on the narrow path.
- Suppressing Fire: wide fan, slow 60% 1.5 s.
- Shoot Em Up: dash to the target, airborne, machine gun while she can't move (long hold), short range.
- Ultimate BRATATATATAT: 5 s more auto damage, rate and range, bullets PIERCE; cancelled by CC.
- Passive: every 10th auto attack bonus damage. Bunny Hop: 5 m. Auto: slow projectile, -30% within 5 m.

## Bumaro — invenglobal.com/articles/2244
- Weapon: slow long-range blob creating a damaging area where it lands; charging adds Mass.
- Cling: attach to an ally/enemy; enemy lower HP: drain, higher HP: slow; ally lower HP: heal, higher: speed.
- Acidic Burst: AoE around him, more Mass spent = more damage and radius.
- Cell Division: channel restoring HP and Mass, more defense, no CC immunity.
- Rubberize: reflects damage (scales with Mass); after 4 s knocks down enemies within 5 m.
- Passive: low Mass = speed, high Mass = damage/skill power/defense.
- Oozify: liquid 2 s, untargetable, enemies on him slowed.
- Ultimate Mitosis: 3 copies imitating skills (not Cling) and attacks.

## Eirna — invenglobal.com/articles/2229
- Passive Frostbite: hits chill; 3 stacks freeze; freezing restores 150 MP.
- Permafrost Petals: wide line of petals, slow 10% 1 s; press again to detonate them in flight.
- Arctic Storm: storm on her or an ally 4 s, slow 20%, freeze, DoT; persists through tag.
- Frost Spire: spire at a location for 10 s, press again to teleport to it; slow 40% 1 s nearby.
- Shivershard: target within 8 m, 500 dmg, freeze.
- Cold Feet (survival): +ms 3 s, ice trail slowing and freezing 1 s.
- Ultimate Rime Pulse: explosion around her, 575 + 60 per chill stack.

## Pancuga — invenglobal.com/articles/2226
- Punch Out: punch; recast twin punch; third cast triple punch.
- Southpaw Special: hook combo; recast uppercut stunning 0.5 s.
- Work the Body: dash + punches, twice; the second knocks back.
- Light Stepper: back dash (evade 0.7 s); quick recast dashes forward.
- Ultimate Rocket Punch: straight punch launching all enemies hit; charge up to 3 s = up to 18 m and 1.5x damage.
- Passives: +20% weapon attack per hit within 1.5 s after a skill; a successful dodge allows one counter (stun 0.5 s) within 3 s.
- Bob and Weave (survival): dodge 0.5 s, +15% ms 2 s. Consecutive skills trigger stronger combo attacks.

## Death Knight — invenglobal.com/articles/2223
- Skelecrash: straight charge ending in a slash; enemies hit during the charge take 50%.
- Hellfire: cross-shaped flame burst on an area, fast, wide, burn 2 s.
- Skull Crack: stun nearby 1 s; cast 1 readies the next weapon attack, cast 2 releases it; long cast, short range.
- Quickening: less heat buildup, +attack and move speed 8 s.
- Ultimate Unholy Fury: shockwave stun 0.5 s; 6 s less heat, much more attack/move speed, defense, HP regen.
- Passive Dark Pact: small lifesteal once per second; revives once at 0 HP with 30% HP after 3 s (10 min cooldown).
- Ignore Wounds (survival): negates enemy SKILL damage 2 s (not weapon attacks nor CC), then +15% ms.
- No resource: cooldowns only; weapon overheats.

## Arami — invenglobal.com/articles/2204
- Weapon: slow, slightly tracking projectile, high damage.
- Glyph Binding: wide glyph triggered by reactivation or an enemy crossing it, binds.
- Glyph Gravel Fist: small glyph, same triggers, launches airborne.
- Glyph Hinder-Plasm: periodically spawns plasms chasing enemies (damage + 50% slow), not focusing one target.
- Glyph Relocation: swaps places with the glyph, blast where she appears, 2 charges.
- Passive Mana Drain: after a skill hit, the next weapon attack steals 75 MP.
- Doppelganger: illusion pulling enemies; untargetable 0.3 s, stealth 2 s, more ms.
- Ultimate Animate Golem: golem fighting for her; enemies on the glyph are launched.

## Lorraine — invenglobal.com/articles/2201
- Speed Shooting (toggle): resets heat, -50% heat buildup, +15% damage and ms, 25 MP every 0.5 s.
- Lens Focus: spotlight 15 m radius revealing stealth, slow 30% (once per 3 s per enemy), castable across walls.
- Flashbang: click-to-hit, blind 4 s, slow 50% 2 s.
- F-Stop: circular hitbox following the cursor; stun up to 2 s scaling with proximity to center; 835 dmg; she is slowed while aiming.
- Ultimate Maximum Exposure: square area pulling enemies to its center.
- Passive Flash Step: +100% ms 1 s on hit. B-Roll (survival): 5 m roll toward the cursor.

## Vonak — invenglobal.com/articles/2162
- Unload: concentrated straight barrage. Spray Pattern: widening arc, more bullets per level.
- Whipshot: bullet on a CURVED trajectory hitting behind obstacles.
- Slamshot: knocks the target back while he jumps back.
- Passive Gunslinger: each skill changes stance and resets the overheat gauge; consecutive stance changes: +20% ms, +50% projectile speed, +20% weapon damage; MP use grows while maintained.
- Slide Tackle (survival): knocks down enemies it collides with.
- Ultimate Hail of Bullets: jumps and sprays a wide area below, brief immunity, can move.

## MBA-07 — invenglobal.com/articles/2174
- Weapon: slow long-range splash rockets. Passive: weapon hits on 2+ enemies restore MP.
- Cluster Missiles: homing missiles (no target selection), short range, more missiles per level.
- Disruptor: explosions on both sides, stun nearby (narrow).
- Artillery Barrage: missiles on an area, more damage near the center.
- Skitter Mine: proximity mine tracking enemies, slows, destructible.
- Smoke Screen: smoke cloud; units inside briefly immune to weapon and skill attacks.
- Ultimate Particle Cannon: immobile siege for 10 s, up to 3 long-range wide bombardments.

## Sizuka — invenglobal.com/articles/2164
- Snap Strike: dash to a target and strike, 1 bleed stack, more stun chance from behind.
- Caltrops: radial caltrops (9 at level 6) slowing and bleeding.
- Cloak: stealth 4 s, ends on skill/attack (then -10% ms).
- Rending Blades: burst, +40% weapon damage, bleeding.
- Passive Vicious: killing a master resets cooldowns; bleed up to 5 stacks, more damage per stack.
- Swift Step (survival): short dash toward the cursor, immune during it.
- Ultimate A Thousand Cuts: dashes in and out slashing (single target, multiple hits), invincible, returns to the start spot.

## Demenos (ELECTRO) — invenglobal.com/articles/2181
- Weapon: mid-range electric attack, can slow; can heal allies.
- Paralyzing Bolt: travelling bolt to a spot (not hitscan), small area, paralyzes (shock).
- Rolling Thunder: fan-shaped field, targets hit can't dodge or cast for 2 s, quick cast.
- Shock Therapy: ally buff 8 s (ms, HP and MP regen), ends if the ally moves too far.
- Ball Lightning: orb moving toward nearby enemies, blocks projectiles, discharges every 2 s, lasts 6 s, explodes at expiry.
- Recharge: heal over time (HP and MP). Lightning Rush (survival): becomes a sphere, enemies hit shocked 0.5 s.
- Ultimate Stasis Storm: destroys projectiles, slowing field, explodes after 2.5 s for massive damage.

## Tulam — invenglobal.com/articles/2173
- Knuckle Duster: whirlwind damaging around, reflects projectiles, can move slowly.
- Landslide: slides to a target and flings it behind him (ally or enemy).
- Tunnel Strike: underground (untargetable), bursts up launching enemies.
- Weight of Stone: around him, -weapon damage and skill power (stamina drain at level 6).
- Passive: below 50% HP more weapon damage and defense.
- Stone Faced (survival): defense + super armor 5 s.
- Ultimate Solquake: pulls enemies to him with damage and launches them; farther = longer air time.

## Dr. Raoul — invenglobal.com/articles/2493
- Ripper Beam: beam following the cursor 4 s, up to 610 dmg, can't move, any action cancels, stealth on the last tick.
- Tesla Mines: random pattern, 170 dmg, slow 50% 2 s, 55 EP.
- Optic Disruptor: blast around him, blind 1 s (non-masters feared), no damage.
- Tele-Strike: teleports behind the target, +50% attack speed 2 s, stealth.
- Ultimate Shadow Drone Assault: stealth 3 s, drone left in place, marked area slows 50% 2 s, drones dive after a delay; recast teleports to the area.
- Passive: landing a skill = 1 s stealth; slowed enemies take more weapon damage. Shadow Step: 4.5 m.
- Resource EP: faster regen, lower max than MP.

## Nanurunerk — invenglobal.com/articles/2472
- Passive: +10 RNG per weapon hit and bundle pickup; full meter empowers the next skill; every 7th attack +10 RNG, slow 1 s, more damage.
- Enchantment Stone Bundle: +30% weapon damage and skill power 10 s.
- Supply Bundle: random: 500 shield or +15% attack speed, 10 s; empowered drops a bundle giving 15 ultimate gauge.
- Manastone Bundle: +2% weapon dmg/skill power/crit, stacking, kept through tag, halved on death.
- Trickster Bundle: ally +30% ms 3 s; enemy pickup binds 2 s.
- Ultimate FIRESALE: knocks back nearby enemies, throws every bundle. Deal Chaser: +ms 2 s.

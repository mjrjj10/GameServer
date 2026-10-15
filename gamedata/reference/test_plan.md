# Plan de test des masters

Généré depuis les données du client (SKILL, ActionBase, REMOTE_PC, STATUS_PC, skilltt) et les guides.
`[type non géré]` = type de statut que le serveur n’applique pas encore. Cases à cocher pour le test final.

## Taejin (STRIKER)

- [ ] **Passif — Combat Instinct** 
  - [Veteran] Recovers a small amount of stamina each time you land a weapon attack.
  - serveur : **non géré** (HIT_RECOVER)
- [ ] **Sort 1 — Concussive Blast** (recharge 8.5s, dégâts de base 355)
  - Fires a close-range blast from Taejin's gun, knocking back enemies in a small cone. Taejin is knocked back slightly when using this skill.
    - MOVE MOVE_PRESET_MOVE_NONE
    - `1440010030` NONETARGET BOX LengthX=960 LengthY=960, dégâts, traverse, statuts: PUSH 1.0s
    - GRAPH_MOVE_HORZ -270
- [ ] **Sort 2 — Proximity Grenade** (recharge 15s, dégâts de base 995)
  - Throws a grenade, dealing damage to targets in a small area. If there is no target in the area, the grenade will detonate after {skill:180010040:remote:lifetime} seconds.
    - MOVE MOVE_PRESET_SEE_TARGET
    - `1440010051` NONETARGET SPHERE LengthX=300, sans dégâts, s’arrête au 1er, statuts: BUSH_REVEAL 3.0s **[type non géré]**
      - `1440010053` NONETARGET SPHERE LengthX=500, dégâts, traverse
- [ ] **Sort 3 — Explosive Barrage** (recharge 12s, dégâts de base 550)
  - Fires a line of explosive shells, each one dealing damage on a successful hit. Taejin is knocked back slightly when using this skill.
    - `1440010065` NONETARGET SPHERE LengthX=300, dégâts, traverse
    - GRAPH_MOVE_HORZ -168
- [ ] **Sort 4 — Inspiring Presence** (recharge 28s)
  - Grants Taejin a {skill:180010030:status:targetingrange} aura that increases weapon damage and movespeed for himself and nearby allies by {skill:180010030:status:stat:mul:attack:1}% for {skill:180010030:status:durationtime}. Aura persists through tagging.
    - statut sur soi/cible : AURA 8.0s
- [ ] **Ultime — Dredgion Strike** (recharge 30s, dégâts de base 680)
  - Calls the Dredgion to bombard an area with pinpoint missile strikes, dealing heavy damage over {skill:180010050:status:durationtime} seconds and slowing enemies by 40%.
    - MOVE MOVE_PRESET_MOVE_NONE
    - `1440010041` NONETARGET SPHERE LengthX=800, sans dégâts, traverse
      - `1440010042` NONETARGET SPHERE LengthX=800, dégâts, traverse, statuts: SLOW 2.0s
- [ ] **Esquive — Roll** (recharge 0.3s)
  - Dive into a roll, granting immunity to crowd control while rolling.
    - statut sur soi/cible : SUPERARMOR 0.5s
    - GRAPH_MOVE_HORZ 600

<details><summary>Guide</summary>

- Passive Combat Instinct: recovers a little stamina on each weapon hit.
- Concussion Blast: close-range small cone, knockback, quick.
- Proximity Grenade: thrown to an area, damage on hit and on explosion, detonates after 3 s if no target, goes off when stepped on.
- Explosive Barrage: line of explosive shells, explodes 4 times, slow projectile.
- Inspiring Presence: +20% weapon damage and move speed for him and allies within 10 m, persists through tagging.
- Ultimate Dredgion Strike: missiles on an area, slow 40% for 2 s, takes time to fire.
- Roll: short roll, CC immune, lots of stamina, 0.3 s cooldown.
</details>

## MBA-07 (ARTILLERY)

- [ ] **Passif — Reactive Explosives** 
  - [Reactive Explosives] If a weapon attack hits two or more enemies at the same time, it instantly restores MP to MBA-07. Against monsters: Recovers 125 MP Against Masters: Recovers 225 MP
  - serveur : **non géré** (ARTILLERY)
- [ ] **Sort 1 — Artillery Barrage** (recharge 12s, dégâts de base 540)
  - Unleashes a battery of missiles at the targeted area. Damage dealt is increased the closer enemies are to the center of the attack area.
    - MOVE MOVE_PRESET_SEE_TARGET
    - `1440020010` GRAPH BOX LengthX=36 LengthY=60 portée≈5000, sans dégâts, s’arrête au 1er
      - `1440020011` NONETARGET SPHERE LengthX=400, dégâts, traverse
- [ ] **Sort 2 — Skitter Mine** (recharge 5s, dégâts de base 675)
  - Drops a proximity mine at your footcups. Mines automatically track enemies within 10m and explode on them, dealing damage and causing slow. Mines are destructible.
    - ENTITY 100041020
- [ ] **Sort 3 — Disruptor** (recharge 15s, dégâts de base 160)
  - Unleashes explosive impacts on either side of MBA-07 that Stun enemies for {skill:180020030:status:durationtime}.
    - statut sur soi/cible : STAT 0.4s
    - `1440020040` NONETARGET BOX LengthX=200 LengthY=600, sans dégâts, traverse
      - `1440020042` NONETARGET BOX LengthX=200 LengthY=600, dégâts, traverse, statuts: STUN 1.5s
    - `1440020041` NONETARGET BOX LengthX=200 LengthY=600, sans dégâts, traverse
- [ ] **Sort 4 — Cluster Missiles** (recharge 15s, dégâts de base 600)
  - Unloads a swarm of {skill:180020040:leveltext:012:5:3456:7} homing missiles that seek out the nearest enemy target. Missiles explode on contact with enemies.
    - `1440020031` HOMING BOX LengthX=60 LengthY=100 portée≈4680, sans dégâts, s’arrête au 1er
      - `1440020032` NONETARGET SPHERE LengthX=250, dégâts, traverse
- [ ] **Esquive — Smoke Screen** (recharge 0.3s)
  - MBA-07 vents its exhaust ports, clouding the surrounding area with smoke. All targets within the smoke are immune to weapon and skill attacks.
    - ENTITY 100041030

<details><summary>Guide</summary>

- Weapon: slow long-range splash rockets. Passive: weapon hits on 2+ enemies restore MP.
- Cluster Missiles: homing missiles (no target selection), short range, more missiles per level.
- Disruptor: explosions on both sides, stun nearby (narrow).
- Artillery Barrage: missiles on an area, more damage near the center.
- Skitter Mine: proximity mine tracking enemies, slows, destructible.
- Smoke Screen: smoke cloud; units inside briefly immune to weapon and skill attacks.
- Ultimate Particle Cannon: immobile siege for 10 s, up to 3 long-range wide bombardments.
</details>

## Sizuka (ASSASSIN)

- [ ] **Passif — Vicious** 
  - [Adrenaline] Killing Masters reset Sizuka's cooldowns. [Bleed them Dry] Bleeding inflicted by Sizuka can stack up to 5 times. Each stack increases damage dealt by the Bleed effect.
  - serveur : **non géré** (ASSASSIN)
- [ ] **Sort 1 — Cloak** (recharge 10s)
  - Enter stealth for {skill:180030010:status:durationtime}. Attacking, using a skill, or dodging will remove stealth. Slowed by 10%.
    - statut sur soi/cible : HIDE 4.0s **[type non géré]**
    - statut sur soi/cible : STAT 4.5s
- [ ] **Sort 2 — Snap Strike** (recharge 3s, dégâts de base 105)
  - Instantly dashes to a target with a disorienting strike. Causes a stack of bleeding with a small chance to stun. If Snap Strike is used while flanking, the chance to stun is greatly increased. Stuns last {skill:180030020:status:durationtime}. Momentarily immune to incoming damage upon casting this s
    - MOVE MOVE_PRESET_GOTO_CURRENT_TARGET
    - statut sur soi/cible : EVADE 0.2s
    - `1440030010` FOLLOW_TARGET SPHERE LengthX=100 portée≈900, dégâts, s’arrête au 1er, statuts: STUN 1.5s, BLEEDING 5.01s
    - statut sur soi/cible : STAT 1.5s
- [ ] **Sort 3 — Caltrops** (recharge 6s, dégâts de base 80)
  - Throws {skill:180030030:leveltext:012:3:345:6:6:9} caltrops at a target area that slows for {skill:180030030:status:durationtime} by {skill:180030030:status:stat:mul:movespeed}% and bleeds enemies who step on them. Sizuka jumps back slightly when using this skill. Momentarily immune to incoming dama
    - statut sur soi/cible : EVADE 0.7s
    - GRAPH_MOVE_HORZ -300
    - MOVE MOVE_PRESET_SEE_TARGET
    - `1440030020` NONETARGET SPHERE LengthX=100, dégâts, s’arrête au 1er, statuts: SLOW 2.0s, BLEEDING 5.01s, BUSH_REVEAL 3.0s **[type non géré]**
- [ ] **Sort 4 — Rending Blades** (recharge 6s)
  - Sizuka focuses her attacks on exposed areas for {skill:180030040:status:durationtime}, increasing her weapon damage by {skill:180030040:status:stat:mul:attack}%, and causes bleeding. Effect stays active, even if tagged.
    - statut sur soi/cible : HIT_PLAGUE 4.0s **[type non géré]**
- [ ] **Ultime — A Thousand Cuts** (recharge 30s, dégâts de base 580)
  - Sizuka dashes in and out of sight, slashing all enemies within range. Sizuka ends her ultimate where it began. Targets can be struck multiple times. When an enemy is repeatedly damaged, successive hits do 10% less damage up to a maximum of 40%.
    - statut sur soi/cible : GODMODE 2.0s
    - statut sur soi/cible : BURROW 2.0s **[type non géré]**
    - `1440030031` TARGET SPHERE LengthX=200 portée≈1500, dégâts, s’arrête au 1er
    - `1440030032` TARGET SPHERE LengthX=200 portée≈1500, dégâts, s’arrête au 1er
    - `1440030033` TARGET SPHERE LengthX=200 portée≈1500, dégâts, s’arrête au 1er
    - `1440030034` TARGET SPHERE LengthX=200 portée≈1500, dégâts, s’arrête au 1er
    - statut sur soi/cible : STAT 5.0s
- [ ] **Esquive — Swift Step** (recharge 0.3s)
  - Instantly dash a short distance in the direction of your cursor.
    - MOVE MOVE_PRESET_WARP

<details><summary>Guide</summary>

- Snap Strike: dash to a target and strike, 1 bleed stack, more stun chance from behind.
- Caltrops: radial caltrops (9 at level 6) slowing and bleeding.
- Cloak: stealth 4 s, ends on skill/attack (then -10% ms).
- Rending Blades: burst, +40% weapon damage, bleeding.
- Passive Vicious: killing a master resets cooldowns; bleed up to 5 stacks, more damage per stack.
- Swift Step (survival): short dash toward the cursor, immune during it.
- Ultimate A Thousand Cuts: dashes in and out slashing (single target, multiple hits), invincible, returns to the start spot.
</details>

## Demenos (ELECTRO)

- [ ] **Passif — Recharge** 
  - [Exhilaration] Recovers MP when healing allies.
  - serveur : **non géré** (ELECTRO)
- [ ] **Sort 1 — Paralyzing Bolt** (recharge 5s, dégâts de base 70)
  - After a short charge, Demenos unleashes a bolt of lightning on the targeted area, shocking enemies for {skill:180040010:status:durationtime}. Enemies closer to the center of the blast are shocked for longer.
    - MOVE MOVE_PRESET_SEE_TARGET
    - `1440040011` NONETARGET SPHERE LengthX=800, dégâts, traverse, statuts: SHOCKED 2.5s
- [ ] **Sort 2 — Rolling Thunder** (recharge 12s, dégâts de base 520)
  - Unleashes a scattered cone of lightning strikes. Enemies caught in the cone are damaged and silenced for {skill:180040020:status:durationtime}.
    - MOVE MOVE_PRESET_MOVE_NONE
    - `1440040020` NONETARGET BOX LengthX=2400 LengthY=2400, sans dégâts, traverse
      - `1440040021` NONETARGET BOX LengthX=2400 LengthY=2400, dégâts, traverse, statuts: SILENCE 2.0s **[type non géré]**
- [ ] **Sort 3 — Shock Therapy** (recharge 10s)
  - Links Demenos to a friendly target with a lightning tether for {skill:180040030:status:durationtime}. Both Demenos and his target gain {skill:180040030:status:stat:mul:movespeed}% movespeed, {skill:180040030:status:stat:mul:defence}% defense, {skill:180040030:status:regen:add:health} HP recovery p/s
    - MOVE MOVE_PRESET_SEE_TARGET
    - statut sur soi/cible : STAT 4.0s
    - statut sur soi/cible : LINK 8.0s **[type non géré]**
- [ ] **Sort 4 — Ball Lightning** (recharge 10s, dégâts de base 455)
  - Creates an orb that destroys projectiles and moves towards the closest enemy. Demenos may chain his attack through the orb, and every 2 seconds the orb will release a wave of charged bolts around itself. The orb explodes for additional damage after {skill:180040040:creature:lifetime}.
    - ENTITY 100041031
    - MOVE MOVE_PRESET_SEE_TARGET
    - statut sur soi/cible : STAT
- [ ] **Ultime — Stasis Storm** (recharge 5s, dégâts de base 940)
  - Distorts a space with a {skill:180040050:remote:range} radius that destroys projectiles. Targets in the field have their muscles tightened, slowing them by {skill:180040050:status:stat:mul:movespeed}%. After {skill:180040050:remote:lifetime}, the field reaches its maximum charge and explodes, causin
    - MOVE MOVE_PRESET_SEE_TARGET
    - `1440040050` NONETARGET SPHERE LengthX=820, sans dégâts, traverse, statuts: STAT 0.8s
      - `1440040051` NONETARGET SPHERE LengthX=820, dégâts, traverse
- [ ] **Esquive — Lightning Rush** (recharge 0.3s)
  - Demenos rushes forward 6.7m and shocks any enemy he collides with for {skill:180040002:status:durationtime}.
    - `1440040060` NONETARGET SPHERE LengthX=300, sans dégâts, traverse, statuts: SHOCKED 0.5s
    - GRAPH_MOVE_HORZ 675

<details><summary>Guide</summary>

- Weapon: mid-range electric attack, can slow; can heal allies.
- Paralyzing Bolt: travelling bolt to a spot (not hitscan), small area, paralyzes (shock).
- Rolling Thunder: fan-shaped field, targets hit can't dodge or cast for 2 s, quick cast.
- Shock Therapy: ally buff 8 s (ms, HP and MP regen), ends if the ally moves too far.
- Ball Lightning: orb moving toward nearby enemies, blocks projectiles, discharges every 2 s, lasts 6 s, explodes at expiry.
- Recharge: heal over time (HP and MP). Lightning Rush (survival): becomes a sphere, enemies hit shocked 0.5 s.
- Ultimate Stasis Storm: destroys projectiles, slowing field, explodes after 2.5 s for massive damage.
</details>

## Koom (DEFENDER)

- [ ] **Passif — Unstoppable Will** 
  - [Unstoppable Will] Koom gains Will when he deals or receives damage. Koom heals 3% of his total HP and gains CC immunity for 1 second on skill activation.
  - serveur : **non géré** (DEFENDER)
- [ ] **Sort 1 — Magnetic Barrier** (recharge 14s)
  - Creates a charged field of stones in front of Koom that blocks projectiles for {skill:180050010:remote:lifetime} and grants 20% Movespeed for 3 seconds. Cannot block area-based attacks.
    - `1440050050` NONETARGET BOX LengthX=400 LengthY=150, sans dégâts, traverse
    - statut sur soi/cible : SUPERARMOR 1.0s
    - statut sur soi/cible : STAT 3.0s
- [ ] **Sort 2 — Runeslab Barricade** (recharge 14s)
  - Koom causes a slab of stone to burst from the target area, blocking enemy attacks and acting as a movement obstruction. Nearby monsters will be forced to attack the wall. Lasts {skill:180050020:creature:lifetime}.
    - statut sur soi/cible : SUPERARMOR 1.0s
    - `1440050031` NONETARGET BOX LengthX=550 LengthY=200, sans dégâts, traverse, statuts: FORCEBACK 0.1s, alliés: FORCEBACK 0.1s
- [ ] **Sort 3 — Unstable Shell** (recharge 24s)
  - Creates a shield around Koom for a short duration, absorbing {skill:180050030:status:stat:add:SHIELD_DEFENCE} damage. After the shield's duration expires, or if the shield is broken, it explodes and deals {skill:180050030:status:explodebasedamage} damage to all enemies within {skill:180050030:status
    - statut sur soi/cible : SUPERARMOR 0.3s
    - statut sur soi/cible : SHIELDBOMB 3.0s **[type non géré]**
- [ ] **Sort 4 — Crashing Defense** (recharge 22s, dégâts de base 55)
  - Koom charges to the targeted area, shielding all nearby allies for {skill:180050040:status:stat:add:SHIELD_DEFENCE} damage at the end of his charge. Lasts {skill:180050040:status:durationtime}. The shield persists through tagging.
    - MOVE MOVE_PRESET_TARGET_POS
    - GRAPH_MOVE_HORZ 900
    - statut sur soi/cible : SUPERARMOR 1.0s
    - statut sur soi/cible : SHIELD 2.5s
    - `1440050010` NONETARGET SPHERE LengthX=1000, dégâts, traverse, alliés: SHIELD 2.5s
- [ ] **Ultime — Impervious Stone** (recharge 50s)
  - Koom becomes undying for {skill:180050050:status:durationtime}, after which, he recovers {skill:180050050:status:paramratio}% of his maximum HP. Koom can recover an additional {skill:180050050:status:maxrecoverhp} HP, based on damage taken while Impervious Stone is active.
    - statut sur soi/cible : SUPERARMOR 1.0s
    - statut sur soi/cible : NEARLY_IMMORTAL 4.0s
- [ ] **Esquive — Granite Sheath** (recharge 0.3s)
  - The exterior of Koom's flesh becomes coated in a thin layer of granite, absorbing the next {skill:180050002:status:stat:add:SHIELD_DEFENCE} damage and recovers 20 Will. For {skill:180050002:status:durationtime}, Koom gains 15% movespeed. The movespeed bonus is lost if the shield breaks.
    - statut sur soi/cible : SHIELD 2.0s
    - statut sur soi/cible : STAT 2.0s
    - MOVE MOVE_PRESET_MOVE_NONE

<details><summary>Guide</summary>

- Weapon does not overheat.
- Magnetic Barrier: frontal shield blocking projectiles 4 s (follows him, fixed facing), +20% ms 3 s; not AoE.
- Runeslab Barricade: stone wall at an area, blocks attacks and movement, minions attack it.
- Unstable Shell: shield; explodes when broken or expired, damage scales with absorbed damage; persists through tag.
- Crashing Defense: charges to an area, shields himself and allies in range.
- Ultimate Impervious Stone: undying 4 s; then +20% max HP plus more based on damage taken.
- Passive: gains Will (resource) when dealing or taking damage, decays over time; 3% HP; 1 s CC immunity on skill use.
- Granite Sheath (survival): 400 shield, +15% ms, +20 Will.
</details>

## Innowin (SNIPER)

- [ ] **Passif — Perfect Alignment** 
  - [Perfect Alignment] Each time Weapon Damage pierces an opponent, the chance for a critical hit to occur is increased by 33%.
  - serveur : **non géré** (SNIPER)
- [ ] **Sort 1 — Energy Lance** (recharge 12s, dégâts de base 970)
  - Unleashes a massive discharge of energy from Innowin's rifle, damaging all enemies in a line.
    - `1440060050` NONETARGET BOX LengthX=200 LengthY=1800, sans dégâts, traverse
    - `1440060051` NONETARGET BOX LengthX=200 LengthY=1800, dégâts, traverse
    - GRAPH_MOVE_HORZ -100
- [ ] **Sort 2 — Shot Caller** (recharge 15s)
  - Calls out a specific target, increasing their chance to be critically struck by allies. While called out, the target won't be able to stealth for {skill:180060010:status:durationtime}.
    - `1440060020` TARGET SPHERE LengthX=90 portée≈3360, dégâts, s’arrête au 1er, statuts: WEAK_ON_CRITICAL 5.0s **[type non géré]**, DISPEL 5.0s, ACTIONBLOCK 5.0s
    - GRAPH_MOVE_HORZ -314
- [ ] **Sort 3 — Explosive Escape** (recharge 15s, dégâts de base 525)
  - Fires an explosive shot that knocks enemies back and launches Innowin into the air, allowing her to briefly dodge hostile attacks.
    - statut sur soi/cible : EVADE 0.5s
    - `1440060030` NONETARGET SPHERE LengthX=650, dégâts, traverse, statuts: FORCEBACK 0.2s
- [ ] **Sort 4 — Paralyzer** (recharge 10s, dégâts de base 115)
  - Fires an energized shot that binds the first target in a line for {skill:180060030:status:durationtime}.
    - `1440060040` NONETARGET BOX LengthX=90 LengthY=48 portée=1000, dégâts, s’arrête au 1er, statuts: ROOT 2.0s
    - GRAPH_MOVE_HORZ -160
- [ ] **Esquive — Reposition** (recharge 0.3s)
  - Quickly dashes 3.6m.
    - GRAPH_MOVE_HORZ 375
- [ ] **Sort 19 — Zeroing In** (recharge 15s, dégâts de base 1590)
  - Innowin enters stealth and is unable to move while taking aim, but gains crowd control immunity. A reticle appears, allowing Innowin to track enemies and fire at the perfect moment.
    - statut sur soi/cible : STAT 10.0s
    - `1440060015` NONETARGET SPHERE LengthX=380, dégâts, s’arrête au 1er

<details><summary>Guide</summary>

- Weapon Nova: -30% damage within 5 m; charged (3-slot battery).
- Energy Lance: delayed (few s) line hitting all targets in it.
- Shot Caller: target can't stealth 5 s, more crit taken.
- Explosive Escape: explosion at her position launching her airborne; enemies 525 dmg + knockback; no damage taken while airborne.
- Paralyzer: straight projectile, immobilize 2 s; lvl 3 pierces.
- Ultimate Zeroing In: stealth + reticle following the cursor slowly, 1590 dmg, CC immune while stealthed, kill refunds ult.
- Passive: +33% crit after hitting an enemy. Survival: 3.6 m dash.
</details>

## Death Knight (DEATHKNIGHT)

- [ ] **Passif — Dark Pact** 
  - [Syphoning Blade] Death Knight's next attack recovers a small amount of HP once each second. [Forever Damned] Death Knight will revive once upon reaching 0 HP. After 3 sec, Death Knight respawns with 30% total HP. Activation triggers a 5 min cool down, and the Death Knight is unable to move or attac
  - serveur : **non géré** (ACTIVATE_UNDEAD)
- [ ] **Sort 1 — Skelecrash** (recharge 14s, dégâts de base 310)
  - Charges in a line, damaging enemies and unleashing a slash that knocks foes away at the end of the charge. Enemies struck by the movement portion of the charge take 50% reduced damage.
    - `1440070020` NONETARGET BOX LengthX=300 LengthY=100, dégâts, traverse
    - `1440070021` NONETARGET BOX LengthX=300 LengthY=350, dégâts, traverse, statuts: PUSH 0.3s
    - GRAPH_MOVE_HORZ 700
- [ ] **Sort 2 — Hellfire** (recharge 10s, dégâts de base 375)
  - Coaxes a cross-shaped burst of flames from the target area, dealing moderate damage with each line of flame. Each line of fire inflicts a stacks of burning on hit enemies for {skill:180070020:status:durationtime}.
    - `1440070030` NONETARGET BOX LengthX=800 LengthY=800, sans dégâts, traverse
    - `1440070031` NONETARGET BOX LengthX=800 LengthY=200, dégâts, traverse, statuts: BURNING 2.0s
- [ ] **Sort 4 — Quickening** (recharge 14s)
  - Charges your bones with devilish power, increasing Attack Speed and Movespeed for {skill:180070040:status:durationtime}.
    - statut sur soi/cible : FIREDELAY 6.0s **[type non géré]**
    - statut sur soi/cible : STAT 6.0s
- [ ] **Ultime — Unholy Fury** (recharge 30s)
  - Emits a burst of power that stuns enemies within {skill:180070050:remote:range} for {skill:180070050:status:durationtime:1}. Greatly increases Attack Speed, Movespeed, Defense, and HP Recovery for {skill:180070050:status:durationtime}, but reduces your base attack damage by 10%.
    - statut sur soi/cible : FIREDELAY 6.0s **[type non géré]**
    - statut sur soi/cible : HEAL 6.0s
    - `1440070050` NONETARGET SPHERE LengthX=900, sans dégâts, traverse, statuts: STUN 0.5s
- [ ] **Esquive — Ignore Wounds** (recharge 0.3s)
  - Death Knight shrugs off all damage from Master skills, but not weapon attacks or crowd control, for {skill:180070002:status:durationtime}. After Ignore Wounds fades, Death Knight gains 15% movespeed for 2 seconds.
    - statut sur soi/cible : IMMUNESKILL 2.0s **[type non géré]**
    - statut sur soi/cible : STAT 2.0s
- [ ] **Sort 20 — Skull Crack** (recharge 9s, dégâts de base 100)
  - Death Knight readies a debilitating strike for his next attack within 5 seconds, causing it to stun for {skill:180070030:status:durationtime}.
    - `1440070040` NONETARGET BOX LengthX=300 LengthY=400, dégâts, traverse, statuts: STUN 1.0s

<details><summary>Guide</summary>

- Skelecrash: straight charge ending in a slash; enemies hit during the charge take 50%.
- Hellfire: cross-shaped flame burst on an area, fast, wide, burn 2 s.
- Skull Crack: stun nearby 1 s; cast 1 readies the next weapon attack, cast 2 releases it; long cast, short range.
- Quickening: less heat buildup, +attack and move speed 8 s.
- Ultimate Unholy Fury: shockwave stun 0.5 s; 6 s less heat, much more attack/move speed, defense, HP regen.
- Passive Dark Pact: small lifesteal once per second; revives once at 0 HP with 30% HP after 3 s (10 min cooldown).
- Ignore Wounds (survival): negates enemy SKILL damage 2 s (not weapon attacks nor CC), then +15% ms.
- No resource: cooldowns only; weapon overheats.
</details>

## Cagnazzo (DESTROYER)

- [ ] **Passif — Fever Pitch** 
  - [Rage Beast] Gains Rage, or RP, when dealing or receiving damage. All Cagnazzo's skills activate empowered effects by consuming a full RP bar on use.
  - serveur : **non géré** (aucune donnée)
- [ ] **Sort 1 — Ripline** (recharge 12s, dégâts de base 690)
  - Cagnazzo hurls his hook in a line, damaging struck enemies and dragging them back to Cagnazzo. [Enraged] Increases damage by 30% and width by 0.8m.
    - `1440080020` NONETARGET BOX LengthX=90 LengthY=120 portée=1000, dégâts, s’arrête au 1er, statuts: DRAGGED 0.3s
- [ ] **Sort 2 — Savage Leap** (recharge 15s, dégâts de base 70)
  - Cagnazzo leaps to the target area, damaging and stunning all enemies he lands on for {skill:180080020:status:durationtime}. The damage and stun duration is increased closer to the center of the area. [Enraged] Deals 30% additional damage and removes the distance-based stun and damage reductions.
    - `1440080010` NONETARGET SPHERE LengthX=800, dégâts, traverse, statuts: STUN 1.0s
    - MOVE MOVE_PRESET_TARGET_POS_LEAP
    - GRAPH_MOVE_HORZ 1000
    - statut sur soi/cible : SUPERARMOR 1.0s
- [ ] **Sort 3 — Terrorize** (recharge 15s, dégâts de base 65)
  - Unleashes a battle roar that terrifies enemies within {skill:180080030:remote:range} for {skill:180080030:status:durationtime}. [Enraged] Deals 30% additional damage and causes terrified targets to take increased damage.
    - `1440080032` NONETARGET SPHERE LengthX=900, sans dégâts, traverse
    - `1440080030` NONETARGET SPHERE LengthX=900, dégâts, traverse, statuts: FEAR 1.5s
    - MOVE MOVE_PRESET_MOVE_NONE
- [ ] **Sort 4 — Crushing Swing** (recharge 12s, dégâts de base 315)
  - Brings down Karith's Fist in a heaving overhead swing, smashing enemies into the ground. [Enraged] Deals 30% additional damage and depletes target's stamina.
    - `1440080040` NONETARGET SPHERE LengthX=500, dégâts, traverse, statuts: KNOCKDOWN 0.75s
- [ ] **Esquive — Charge** (recharge 0.3s)
  - Charge forward 6.7m while ignoring crowd control effects.
    - statut sur soi/cible : SUPERARMOR 1.0s
    - GRAPH_MOVE_HORZ 675

<details><summary>Guide</summary>

- Passive Fever Pitch: gains RAGE when dealing OR receiving damage; full rage empowers the next skill and consumes the bar.
- Weapon: whirling swing hitting all enemies in range.
- Ripline: hook in a line, drags the FIRST struck opponent back, 690 dmg; lvl 3 drags several; empowered +30% dmg, +0.8 m width.
- Savage Leap: leap to an area, 70 dmg, stun 1 s, damage/stun decrease with distance from center, cd 12 s.
- Terrorize: 9 m radius, fear 2 s.
- Crushing Swing: knockdown, 315 dmg; lvl 6 binds 3.5 s.
- Barbaric Frenzy: 7 s CC immunity, more weapon/skill damage and defense.
- Charge (survival): charges forward ignoring CC.
</details>

## Moro (MECHANIC)

- [ ] **Passif — Machine Head** 
  - [Robomoro] May have a maximum of 3 summoned robos at any given time. Firing Moro's weapon attacks at friendly Robos heals them and restores Stamina to allied Masters. Does not heal enemy robots. [Robo Go Go] Moro gains passive buffs when deploying Robos. Robos drop a mana battery on death that resto
  - serveur : **non géré** (MECHANIC)
- [ ] **Esquive — Panic!** (recharge 0.3s)
  - Moro flees in a panic for about 9m.
    - GRAPH_MOVE_HORZ 900

<details><summary>Guide</summary>

- Robos are static, lose HP over time and when hit; deploying costs mana; more robos = more damage; dead robos drop a mana battery.
- Rocket Robo: attacks nearby enemies; Moro +100% projectile speed 5 s.
- Carbo-gel Robo: gel area disabling enemy weapon attacks; Moro +5% weapon dmg 5 s per enemy skill cast.
- Graviton Robo: LINE area slowing enemies passing through; Moro's attacks slow 30% for 5 s.
- Medi-Robo: heals allies in a small radius; heals Moro instantly.
- Ultimate Mecha-Moro: exo-suit, homing missiles every 1 s, healing/graviton aura, interrupts enemies; Moro slowed.
- Weapon: slow charged projectile healing robos (splash) or restoring ally stamina. Panic (survival): ~9 m.
</details>

## Arami (SOULMASTER)

- [ ] **Passif — Mana Drain** 
  - [Drain] Successfully hitting enemies with Arami's skills marks targets briefly, allowing her next weapon attack to steal 75 MP from them.
  - serveur : géré (MANABURN)
- [ ] **Passif — Passive Teleport** 
  - Teleports.
  - serveur : **non géré** (TARGET_TELEPORT)
- [ ] **Esquive — Doppelganger** (recharge 0.3s)
  - Creates an illusion that pulls enemies towards it. When the illusion is created, Arami becomes untargetable for 0.3 seconds and stealthed for 2 seconds. +100% movespeed while stealthed.
    - statut sur soi/cible : HIDE 2.0s **[type non géré]**
    - statut sur soi/cible : EVADE 0.3s
    - statut sur soi/cible : STAT 0.3s
    - ENTITY 100033840
- [ ] **Sort 21 — Glyph: Binding** (recharge 12s)
  - After a brief delay, enemies crossing the glyph will trigger it and become bound. The glyph lasts for {skill:180100010:remote:lifetime}. [Combo] Instantly activates the glyph.
    - `1440100011` NONETARGET SPHERE LengthX=700, sans dégâts, traverse, statuts: ROOT 2.0s, STAT 5.0s
      - `1440100012` NONETARGET SPHERE LengthX=700, sans dégâts, traverse, statuts: TOGGLE **[type non géré]**
- [ ] **Sort 22 — Glyph: Gravel Fist** (recharge 8s, dégâts de base 300)
  - A rock fist bursts out of the glyph, launching enemies who trigger the glyph into the air while inflicting damage. [Combo] Instantly activates the glyph.
    - `1440100021` NONETARGET SPHERE LengthX=400, dégâts, traverse, statuts: AIRBORNE 0.75s, STAT 5.0s
- [ ] **Sort 23 — Glyph: Hinder-Plasm** (recharge 12s, dégâts de base 310)
  - Periodically produces Hinder-Plasm that chases enemies, damaging and sticking to them, slowing them by {skill:180100030:status:stat:mul:movespeed}%. Multiple plasms will not focus the same target
- [ ] **Sort 24 — Glyph: Relocation** (recharge 18s, dégâts de base 385)
  - Arami creates a teleportation Glyph. [Combo] Arami and the Glyph switch places, causing a damaging blast where Arami appears. The seal can be used twice before fading.
    - ENTITY 100033843

<details><summary>Guide</summary>

- Weapon: slow, slightly tracking projectile, high damage.
- Glyph Binding: wide glyph triggered by reactivation or an enemy crossing it, binds.
- Glyph Gravel Fist: small glyph, same triggers, launches airborne.
- Glyph Hinder-Plasm: periodically spawns plasms chasing enemies (damage + 50% slow), not focusing one target.
- Glyph Relocation: swaps places with the glyph, blast where she appears, 2 charges.
- Passive Mana Drain: after a skill hit, the next weapon attack steals 75 MP.
- Doppelganger: illusion pulling enemies; untargetable 0.3 s, stealth 2 s, more ms.
- Ultimate Animate Golem: golem fighting for her; enemies on the glyph are launched.
</details>

## Vonak (DOUBLEGUN)

- [ ] **Passif — Gunslinger** 
  - [Firing Stance] Using skills changes Vonak's stance and ammunition, transforming his weapon attack method. Changing firing stances resets Vonak's overheat gauge. [Stance Dance] Repeatedly changing stances within a short period of time gives stacking bonuses, up to 3 times. Each stance change increas
  - serveur : **non géré** (DOUBLEGUN)
- [ ] **Ultime — Hail of Bullets** (recharge 30s, dégâts de base 340)
  - Vonak jumps into the air and sprays bullets in an area under him. Vonak is able to move and is immune to enemy attacks while Hail of Bullets is active.
    - `1440110052` NONETARGET CAPSULE LengthX=500 LengthY=720, dégâts, traverse
    - statut sur soi/cible : EVADE 5.0s
    - MOVE MOVE_PRESET_MOVE_NONE
- [ ] **Esquive — Slide Tackle** (recharge 0.3s)
  - Slides 6.7m and knocks down enemies you collide with.
    - `1440110060` NONETARGET SPHERE LengthX=180, sans dégâts, traverse, statuts: FLY 0.6s
    - GRAPH_MOVE_HORZ 675

<details><summary>Guide</summary>

- Unload: concentrated straight barrage. Spray Pattern: widening arc, more bullets per level.
- Whipshot: bullet on a CURVED trajectory hitting behind obstacles.
- Slamshot: knocks the target back while he jumps back.
- Passive Gunslinger: each skill changes stance and resets the overheat gauge; consecutive stance changes: +20% ms, +50% projectile speed, +20% weapon damage; MP use grows while maintained.
- Slide Tackle (survival): knocks down enemies it collides with.
- Ultimate Hail of Bullets: jumps and sprays a wide area below, brief immunity, can move.
</details>

## Jinsoyun (JINSEOYEON)

- [ ] **Passif — Soul Blade** 
  - [Soul Battery] Jinsoyun's sword acts as a Soul Battery, allowing her to store Soul Blades. Able to harness 5 soul blades at a time. Each time a skill is used, Soul Blades are consumed to grant an additional effect. [Spirit Recovery] A new Soul Blade is created every 2 weapon attacks, regardless of w
  - serveur : **non géré** (JINSEOYEON)
- [ ] **Sort 2 — Rising Blade** (recharge 8s, dégâts de base 200)
  - Launches enemies directly in front of you with an upward swipe, disabling stealth granted by brush for 3 seconds. [Combo: 2 Blades] Must recast to spend Soul Blades. Strike a target with an additional attack while they are launched.
    - `1440120022` NONETARGET BOX LengthX=400 LengthY=350, dégâts, traverse, statuts: AIRBORNE 1.5s, BUSH_REVEAL 3.0s **[type non géré]**, STAT 2.0s
- [ ] **Sort 3 — Blade Rush** (recharge 12s, dégâts de base 685)
  - Charges to the target area and inflicts damage to enemies. [Combo: 2 Blades] Automatically spends Soul Blades. Release an explosion of spiritual energy for additional damage. Targets struck by the explosion are bound for {skill:180120030:status:durationtime}.
    - `1440120031` NONETARGET SPHERE LengthX=480, dégâts, traverse
    - MOVE MOVE_PRESET_TARGET_POS
    - GRAPH_MOVE_HORZ 1000
- [ ] **Sort 4 — Chi Pull** (recharge 16s, dégâts de base 540)
  - Drags the targets in the selected area to you. [Combo: 2 Blades] Automatically spends Soul Blades. If your pull hits a target, and is empowered, you will be healed for 15% max HP.
    - `1440120041` NONETARGET SPHERE LengthX=400, dégâts, traverse, statuts: DRAGGED 0.4s
- [ ] **Ultime — Shock Blade** (recharge 30s, dégâts de base 905)
  - Drops three spiritual blades in a triangle formation. These blades link with beams of lightning that shock and damage enemies they hit. Shock lasts for {skill:180120050:status:durationtime}.
    - `1440120050` NONETARGET BEAM_NIF LengthX=25, dégâts, traverse, statuts: SHOCKED 1.0s
- [ ] **Esquive — Guardian Force** (recharge 0.3s)
  - Surrounds Jinsoyun in protective spiritual pressure for {skill:180120002:status:durationtime}, allowing her to avoid all attacks and reflect projectiles for the duration.
    - statut sur soi/cible : EVADE 0.5s
    - statut sur soi/cible : FLAG 2.0s
    - MOVE MOVE_PRESET_SEE_TARGET
    - `1440120010` NONETARGET BOX LengthX=400 LengthY=150, sans dégâts, traverse
- [ ] **Sort 16 — Air Dash** (recharge 13s)
  - Dashes to a target enemy. May dash twice in quck succession before activating a cool down. [Combo: 2 Blades] Combo activates automatically. The target enemy is crushed by spiritual pressure, and is slowed for {skill:180120010:status:durationtime}.
    - MOVE MOVE_PRESET_GOTO_CURRENT_TARGET_THROUGH
    - statut sur soi/cible : EVADE

<details><summary>Guide</summary>

- Passive Soul Blade: up to 5 stacks, +1 per 2 auto attacks or per dodged attack; 2+ stacks empower skills.
- Air Dash: dash behind the targeted enemy, usable twice; empowered slows. Combo: Rising Blade.
- Rising Blade: airborne on the target, no bush stealth 3 s, short range; empowered extra hit.
- Blade Rush: dash to a ground spot, damage on landing after a short delay; empowered: target can't move 1 s.
- Chi Pull: pulls the target to her, delayed; empowered + hit heals 15% max HP.
- Ultimate Shock Blade: triangle field at a spot, 5 s, only the lightning BEAMS shock (1 s); the center is safe.
- Guardian Force: invincible 0.5 s, deflects projectiles; on success +15% ms 2 s.
</details>

## Kromede (KROMEDE)

- [ ] **Passif — Blood Trial** 
  - [Blood Trial] Dealing weapon damage to stunned, bound, or shocked enemies causes bleeding. Bleeding deals damage over time for 5 seconds.
  - serveur : géré (KROMEDE)
- [ ] **Sort 1 — Quick Draw** (recharge 10s, dégâts de base 210)
  - Quickly fires a cone of five arrows. Bleeding targets take 10% more damage, and consecutive hits with this combo deal 30% additional damage. [Combo] Uses Quick Draw, Rapid Fire, and Fan of Arrows consecutively. - Quick Draw: Kromede fires a single arrow. - Rapid Fire: Kromede fires 3 arrows in a con
    - `1440130010` NONETARGET BOX LengthX=30 LengthY=72 portée=1200, dégâts, s’arrête au 1er
- [ ] **Sort 2 — Galeforce Arrow** (recharge 8s, dégâts de base 420)
  - Kromede fires an arrow that knocks enemies away. If an enemy collides with another enemy while being knocked back, both will be stunned for {skill:180130020:status:durationtime:1}. Deals 10% additional damage to bleeding targets.
    - `1440130040` NONETARGET BOX LengthX=30 LengthY=72 portée=1600, dégâts, s’arrête au 1er, statuts: PUSH 1.0s
- [ ] **Sort 3 — Blot Out the Sun** (recharge 18s, dégâts de base 200)
  - Fires a multitude of arrows in an upwards arc, leaving them to pepper an area over a short duration. Bleeding targets take 10% more damage.
    - `1440130062` NONETARGET SPHERE LengthX=800, dégâts, traverse
- [ ] **Sort 4 — Aetherpike Shards** (recharge 12s)
  - Ruptures the ground with aetheric spikes, binding enemies who touch them. The shards last {skill:180130040:creature:lifetime} and are destructible.
    - ENTITY 100033900
- [ ] **Esquive — Windborne Shield** (recharge 0.3s)
  - Creates a protective shield to block the next {skill:180130002:status:shiledcount} source of damage to hit you. The shield lasts for {skill:180130002:status:durationtime} and grants +50% movespeed.
    - statut sur soi/cible : SHIELD 4.0s
    - statut sur soi/cible : SUPERARMOR 4.0s

<details><summary>Guide</summary>

- Passive Bleed: attacking stunned/bound/shocked enemies makes them bleed 5 s; bleeding enemies take +10% from her skills; her damage -30% within 5 m.
- Galeforce Arrow: single long-range arrow, high damage, knockback; stun 1 s if the target collides with another enemy.
- Aetherpike Shards: line of spikes; stepping on one binds 4.5 s and destroys it; spikes block attacks.
- Quick Draw: up to 3 consecutive casts: 1, 3, then 5 arrows in a fan.
- Blot Out the Sun: arrows on an area, damage over time, +10% on bleeding.
- Ultimate Righteous Justice: 7 s, auto attacks fire 5 arrows in a fan, less heat buildup.
- Windborne Shield: blocks one attack, 4 s, +50% move speed.
</details>

## Jingtai (RODMASTER)

- [ ] **Passif — Staff Fighter** 
  - [Endurance] Restores stamina after using 4 skills. [Cascade of Blows] Each weapon strike increases your weapon damage by 1%. Cascade of blows stacks up to 10%. The weapon attack that ends your string of attacks launches enemies into the air.
  - serveur : **non géré** (SKILLSTAMINA)
- [ ] **Sort 1 — Million Staff Burst** (recharge 2s, dégâts de base 420)
  - Delivers multiple blows in a short line and decreases enemy defenses by {skill:180140010:status:stat:mul:defence}% for {skill:180140010:status:durationtime}. Moving or attacking cancels this skill.
    - MOVE MOVE_PRESET_MOVE_NONE
    - `1440140010` NONETARGET BOX LengthX=280 LengthY=450, dégâts, traverse, statuts: STAT 5.0s
- [ ] **Sort 2 — Sweep Aside** (recharge 5s, dégâts de base 515)
  - Charge forward, pushing enemies away. [Combo] Dash forward again towards a struck enemy.
    - GRAPH_MOVE_HORZ 550
    - statut sur soi/cible : STAT 1.5s
    - `1440140021` NONETARGET BOX LengthX=300 LengthY=350, sans dégâts, s’arrête au 1er
- [ ] **Ultime — Spirit of Protection** (recharge 30s, dégâts de base 860)
  - Jingtai projects a spiritual image of himself. The projection charges forward, spinning its staff and blocking enemy projectiles.
    - `1440140050` NONETARGET BOX LengthX=240 LengthY=120 portée≈1500, dégâts, traverse
      - `1440140051` NONETARGET BOX LengthX=240 LengthY=120, sans dégâts, traverse
- [ ] **Esquive — Pogo** (recharge 0.3s)
  - Jingtai balances on his staff, becoming invincible for {skill:180140002:status:durationtime}. [Combo] Jingtai may reuse this skill to leap away. Moves in the direction you are facing if no directional movement command is given.
    - statut sur soi/cible : EVADE 1.0s
- [ ] **Esquive — Pogo Leap** 
  - Leaps in the target direction. Moves in the direction you are facing if no directional movement command is given.
    - GRAPH_MOVE_HORZ 800
- [ ] **Sort 19 — Bounding Strike** (recharge 8s, dégâts de base 455)
  - Gathers Chi and leaps to the target area, dealing damage to enemies upon landing. Increases the distance leapt the longer you gather Chi.
    - `1440140030` NONETARGET SPHERE LengthX=400, dégâts, traverse
    - MOVE MOVE_PRESET_TARGET_POS
    - GRAPH_MOVE_HORZ 1800
    - statut sur soi/cible : EVADE 1.0s
- [ ] **Sort 21 — Fling** (recharge 8s, dégâts de base 65)
  - Dashes to an enemy and flings them over your shoulder.
    - `1440140040` NONETARGET BOX LengthX=400 LengthY=700 portée=800, dégâts, s’arrête au 1er, statuts: ACTIONBLOCK 0.5s, FLY 1.05s, STAT 2.0s
    - MOVE MOVE_PRESET_GOTO_CURRENT_TARGET

<details><summary>Guide</summary>

- Passive: stamina after every 4 skills; +1% weapon damage per strike (max 10%); last strike of the string launches airborne.
- Million Staff Burst: multi-hit short line, -10% target defense.
- Sweep Aside: pushes enemies in the charge direction; second press dashes to the pushed enemy.
- Bounding Strike: leap to area (hold = farther), damage on landing.
- Fling: dashes to an enemy and throws it over the shoulder (knockdown).
- Pogo (survival): invincible 1 s; reuse leaps forward.
- Ultimate Spirit of Protection: projection charging forward, damages along the path, blocks projectiles.
</details>

## Lorraine (PHOTOG)

- [ ] **Passif — Flash Step** 
  - [Flash Step] Skills grant 100% movespeed for 1 second when they strike an enemy.
  - serveur : **non géré** (aucune donnée)
- [ ] **Sort 2 — Lens Focus** (recharge 10s)
  - Bends light into the focused area for {skill:180150020:remote:lifetime}. Enemies in the focused area will be revealed, even if stealthed, and slowed by {skill:180150020:status:stat:mul:movespeed}%. Lens Focus detects Stealth in a radius of 5m. Only one Spotlight can be active at a time. The passive 
    - ENTITY 100033880
    - ENTITY 100033881
    - ENTITY 100033882
    - `1440150013` NONETARGET SPHERE LengthX=600, sans dégâts, traverse
- [ ] **Sort 3 — Flashbang** (recharge 13s)
  - Blinds an enemy for {skill:180150030:status:durationtime} by concentrating the surrounding light through a camera flash. Additionally, Flashbang slows by {skill:180150030:status:stat:mul:movespeed:1}% for {skill:180150030:status:durationtime:1}. Non-Master targets will be terrorized instead.
    - `1440150020` TARGET BOX LengthX=200 LengthY=200 portée≈900, sans dégâts, s’arrête au 1er, statuts: FLASH_HIT 2.0s **[type non géré]**, SLOW 2.0s
- [ ] **Ultime — Maximum Exposure** (recharge 30s, dégâts de base 905)
  - Lorraine establishes a photo zone to gain maximum exposure with her camera. All caught enemies are dragged into the exposure area and take massive damage.
    - `1440150040` NONETARGET BOX LengthX=700 LengthY=700, sans dégâts, traverse, statuts: SLOW 0.9s
      - `1440150042` NONETARGET BOX LengthX=700 LengthY=700, dégâts, traverse, statuts: STUN 0.1s
      - `1440150041` NONETARGET BOX LengthX=700 LengthY=700, sans dégâts, traverse, statuts: MOTION 1.5s **[type non géré]**
    - ENTITY 100033890
    - statut sur soi/cible : SUPERARMOR 2.5s
- [ ] **Esquive — B-Roll** (recharge 0.3s)
  - Quickly rolls 5m in the direction you are moving.
    - GRAPH_MOVE_HORZ 500
- [ ] **Sort 20 — F-Stop** (recharge 12s, dégâts de base 835)
  - Manipulates captured moments in time through the lens of Lorraine's camera to freeze enemies in place. Stuns enemies within the target area for {skill:180150040:status:durationtime}. Slowed while using F-Stop. The stun duration is reduced the further away enemies are from the center of the targeted 
    - `1440150031` NONETARGET SPHERE LengthX=200, sans dégâts, s’arrête au 1er
    - `1440150032` NONETARGET SPHERE LengthX=420, dégâts, traverse, statuts: STUN 2.0s

<details><summary>Guide</summary>

- Speed Shooting (toggle): resets heat, -50% heat buildup, +15% damage and ms, 25 MP every 0.5 s.
- Lens Focus: spotlight 15 m radius revealing stealth, slow 30% (once per 3 s per enemy), castable across walls.
- Flashbang: click-to-hit, blind 4 s, slow 50% 2 s.
- F-Stop: circular hitbox following the cursor; stun up to 2 s scaling with proximity to center; 835 dmg; she is slowed while aiming.
- Ultimate Maximum Exposure: square area pulling enemies to its center.
- Passive Flash Step: +100% ms 1 s on hit. B-Roll (survival): 5 m roll toward the cursor.
</details>

## Eirna (ICEQUEEN)

- [ ] **Passif — Frostbite** 
  - [Frostbite] Skill attacks apply frostbite, as well as every 6th weapon attack. At 3 stacks, Eirna recovers 150MP and freezes her frostbitten target.
  - serveur : géré (ICEQUEEN)
- [ ] **Passif — Passive Teleport** 
  - 
  - serveur : **non géré** (TARGET_TELEPORT)
- [ ] **Sort 3 — Frost Spire** (recharge 15s)
  - Creates a frost spire at the target area for {skill:180160030:creature:lifetime}. The spire radiates cold, slowing enemies by {skill:180160030:status:stat:mul:movespeed}% for {skill:180160030:status:durationtime}. [Combo] Teleport to the location of the spire.
    - statut sur soi/cible : STAT 10.0s
    - ENTITY 100033870
    - `1440160041` NONETARGET SPHERE LengthX=800, sans dégâts, traverse, statuts: COLD 6.0s **[type non géré]**
- [ ] **Ultime — Rime Pulse** (recharge 3s, dégâts de base 745)
  - After a short delay, blasts enemies by flash freezing their already cold bodies in an area around Eirna. Damage increased by 60 per stack of chilling on your targets.
    - `1440160061` NONETARGET SPHERE LengthX=1200, sans dégâts, traverse
    - `1440160060` NONETARGET SPHERE LengthX=1200, dégâts, traverse, statuts: COLD 6.0s **[type non géré]**
- [ ] **Esquive — Cold Feet** (recharge 0.3s)
  - Gain a burst of movespeed for 3 seconds by riding a stream of supercool air. The air chills the ground, leaving frost patches that chill and slow enemies for 1 second.
    - statut sur soi/cible : STAT 3.0s
    - statut sur soi/cible : STAT 1.0s
    - `1440160010` NONETARGET SPHERE LengthX=180, sans dégâts, traverse
      - `1440160011` NONETARGET BOX LengthX=240 LengthY=240, sans dégâts, traverse, statuts: SLOW 1.0s
        - `1440160012` NONETARGET BOX LengthX=240 LengthY=240, sans dégâts, traverse, statuts: COLD 6.0s **[type non géré]**
- [ ] **Sort 21 — Permafrost Petals** (recharge 10s, dégâts de base 386.25)
  - Casts a mass of razorsharp ice petals in a line, slowing struck enemies by {skill:180160010:status:stat:mul:movespeed}% for {skill:180160010:status:durationtime}. [Iceburst] Detonate the frozen petals for additional damage.
    - `1440160020` NONETARGET SPHERE LengthX=400 portée≈1200, dégâts, traverse, statuts: COLD 6.0s **[type non géré]**, SLOW 1.0s
      - `1440160021` NONETARGET BEAM_NIF LengthX=50, dégâts, traverse
- [ ] **Sort 22 — Arctic Storm** (recharge 10s)
  - Whips up a destructive swirl of ice shards around yourself or any allied target for {skill:180160020:remote:lifetime}. Enemies in the storm take periodic damage and are slowed by {skill:180160020:status:stat:mul:movespeed:1}% for 5 seconds. Persists through tagging.
    - `1440160030` NONETARGET SPHERE LengthX=800, sans dégâts, traverse, statuts: COLD 6.0s **[type non géré]**
    - statut sur soi/cible : AURA 4.0s
    - statut sur soi/cible : BUSH_REVEAL 4.0s **[type non géré]**
- [ ] **Sort 24 — Shivershard** (recharge 5s, dégâts de base 500)
  - Shoots a targeted ice shard that damages, chills, and has a low cooldown.
    - `1440160050` TARGET BOX LengthX=90 LengthY=90 portée≈2625, dégâts, s’arrête au 1er, statuts: COLD 6.0s **[type non géré]**

<details><summary>Guide</summary>

- Passive Frostbite: hits chill; 3 stacks freeze; freezing restores 150 MP.
- Permafrost Petals: wide line of petals, slow 10% 1 s; press again to detonate them in flight.
- Arctic Storm: storm on her or an ally 4 s, slow 20%, freeze, DoT; persists through tag.
- Frost Spire: spire at a location for 10 s, press again to teleport to it; slow 40% 1 s nearby.
- Shivershard: target within 8 m, 500 dmg, freeze.
- Cold Feet (survival): +ms 3 s, ice trail slowing and freezing 1 s.
- Ultimate Rime Pulse: explosion around her, 575 + 60 per chill stack.
</details>

## Vita (MAGICGIRL)

- [ ] **Passif — Harmony** 
  - [Harmony] On every third skill use, Vita creates a shield for 6 sec that absorbs damage. Vita's Harmony shield restores 50 MP per sec and blocks 400 damage. When the Harmony shield expires, Vita’s skills have their cooldown reduced by 20%. Shield persists through tagging.
  - serveur : géré (SKILLSTATUS)
- [ ] **Sort 1 — Wooly Ballad** (recharge 18s)
  - Vita's voice carries a magical tune, polymorphing her target for {skill:180170010:status:durationtime}, and applying a {skill:180170010:status:stat:mul:movespeed:1}% slow.
    - `1440170010` TARGET BOX LengthX=90 LengthY=90 portée≈2300, sans dégâts, s’arrête au 1er, statuts: MORPH 1.25s **[type non géré]**, SLOW 1.25s
- [ ] **Sort 2 — Note of Nullification** (recharge 10s, dégâts de base 190)
  - Vita hits a note that resonates with the target. The effect differs, depending on the target. Cannot be used on creatures summoned by allies. Ally Effect: Shields {skill:180170020:status:stat:add:shield_defence} damage for {skill:180170020:status:durationtime}. Persists through tagging. Enemy Effect
    - `1440170020` TARGET BOX LengthX=90 LengthY=90 portée≈2300, dégâts, s’arrête au 1er, statuts: DISPEL 0.1s, alliés: SHIELD 7.0s
- [ ] **Sort 3 — Valiabolt** (recharge 12s, dégâts de base 315)
  - Fires a bolt that absorbs or grants life energy depending on its target. Grants {skill:180170030:status:regen:add:health:1} HP recovery to allies, while enemies are dealt damage and slowed by 50% for {skill:180170030:status:durationtime}. Can pierce up to 4 targets.
    - `1440170030` NONETARGET BOX LengthX=210 LengthY=120 portée≈2700, dégâts, traverse, statuts: SLOW 2.5s, alliés: HEAL 0.1s
- [ ] **Sort 4 — Vital Aura** (recharge 10s, dégâts de base 260)
  - Wraps Vita in an aura of living energy that grants +{skill:180170040:status:stat:mul:movespeed:2}% movespeed for {skill:180170040:status:durationtime:2}. Effect on ally: Allies are healed for {skill:180170040:status:regen:add:health} HP. Effect on enemy: Enemies are damaged and silenced for {skill:1
    - `1440170040` ORBIT SPHERE LengthX=99, dégâts, s’arrête au 1er, statuts: SILENCE 0.5s **[type non géré]**, BUSH_REVEAL 3.0s **[type non géré]**, alliés: HEAL
    - `1440170041` ORBIT SPHERE LengthX=99, dégâts, s’arrête au 1er, statuts: SILENCE 0.5s **[type non géré]**, BUSH_REVEAL 3.0s **[type non géré]**, alliés: HEAL
    - `1440170042` ORBIT SPHERE LengthX=99, dégâts, s’arrête au 1er, statuts: SILENCE 0.5s **[type non géré]**, BUSH_REVEAL 3.0s **[type non géré]**, alliés: HEAL
    - `1440170043` ORBIT NONE , dégâts, s’arrête au 1er
    - statut sur soi/cible : STAT 4.0s
- [ ] **Ultime — Aria of Life** (recharge 30s)
  - Vita amplifies her control over life energy, allowing her to deny any degradation of life. All allies and enemies within {skill:180170050:remote:range} cannot attack for {skill:180170050:status:durationtime} and become invincible, while allies in the area are healed for {skill:180170050:status:regen
    - statut sur soi/cible : STAT 5.0s
    - `1440170050` NONETARGET SPHERE LengthX=1000, sans dégâts, traverse
    - ENTITY 100033910
- [ ] **Esquive — Dizzying Twirl** (recharge 0.3s)
  - Quickly moves 5m with a spin.
    - GRAPH_MOVE_HORZ 500

<details><summary>Guide</summary>

- Passive Harmony: every 3rd skill a shield for 6 s (absorbs damage, regenerates MP); on expiry -30% skill cooldowns.
- Wooly Ballad: click-to-hit polymorph into a lamb, slowed 30%.
- Note of Nullification: ally: shield blocking 400 damage; enemy: removes buffs, damage per buff removed.
- Valiabolt: long-range projectile PIERCING several targets; allies hit healed 250, enemies damaged + slowed 50%.
- Vital Aura: hearts orbiting close around her, move speed buff; allies touching healed 100, enemies damaged + silenced.
- Dizzying Twirl: evades attacks with a quick turn.
- Ultimate Aria of Life: within 5 m, cancels enemy attacks, nobody can attack or move, all immune; allies recover HP/stamina.
</details>

## Poharan (POHWARAN)

- [ ] **Passif — Ammo Pack** 
  - [Lucky Bullet] Fires a special bullet with every 10th weapon attack, dealing additional damage.
  - serveur : **non géré** (aucune donnée)
- [ ] **Sort 1 — Blitz Grenade** (recharge 1s, dégâts de base 575)
  - Fires a Blitz Grenade, causing an explosion in the target location and leaving a patch of fire for {skill:180180010:remote:lifetime}. The grenade and flame patch burns enemies for {skill:180180010:status:durationtime}, and burning can stack up to 5 times. [Resupply] Supplies Blitz Grenades. Restocks
    - `1440180010` GRAPH SPHERE LengthX=100 portée≈3000, sans dégâts, s’arrête au 1er
      - `1440180011` NONETARGET SPHERE LengthX=360, dégâts, traverse
        - `1440180012` NONETARGET SPHERE LengthX=360, sans dégâts, traverse, statuts: BURNING 2.0s
- [ ] **Sort 2 — Gatling Jump** (recharge 15s, dégâts de base 450)
  - Leaps over and damages a target area in a hail of gunfire. Cannot leap through walls.
    - `1440180020` NONETARGET BOX LengthX=222 LengthY=222, dégâts, traverse
    - GRAPH_MOVE_HORZ 1000
- [ ] **Sort 3 — Suppressing Fire** (recharge 12s, dégâts de base 390)
  - Sprays bullets in a large cone, damaging and slowing enemies by {skill:180180030:status:stat:mul:movespeed}% for {skill:180180030:status:durationtime}.
    - MOVE MOVE_PRESET_MOVE_NONE
    - `1440180030` NONETARGET BOX LengthX=1200 LengthY=2800, sans dégâts, traverse
    - `1440180031` NONETARGET BOX LengthX=1200 LengthY=2800, dégâts, traverse, statuts: SLOW 1.5s
- [ ] **Sort 4 — Shoot Em Up** (recharge 18s, dégâts de base 179.399994)
  - Charges and launches the first enemy that she collides with into the air. Automatically fires a slew of bullets into the launched enemy.
    - GRAPH_MOVE_HORZ 550
    - `1440180042` NONETARGET BOX LengthX=240 LengthY=250, sans dégâts, s’arrête au 1er
- [ ] **Esquive — Bunny Hop** (recharge 0.3s)
  - Quickly hops 5m.
    - GRAPH_MOVE_HORZ 500

<details><summary>Guide</summary>

- Blitz Grenade: explodes on landing, then a small burning area (DoT).
- Gatling Jump: jumps forward, slight damage on the narrow path.
- Suppressing Fire: wide fan, slow 60% 1.5 s.
- Shoot Em Up: dash to the target, airborne, machine gun while she can't move (long hold), short range.
- Ultimate BRATATATATAT: 5 s more auto damage, rate and range, bullets PIERCE; cancelled by CC.
- Passive: every 10th auto attack bonus damage. Bunny Hop: 5 m. Auto: slow projectile, -30% within 5 m.
</details>

## V-Merang (BOOMERANG)

- [ ] **Passif — Sametan Warrior** 
  - [Ringblade Launcher] Consumes 1 Ringblade per skill use. Up to 2 can be stored and 1 Ringblade is restored on every tenth hit with a skill. Multiple hits from the same skill count. [Hindering Blade] Every 8 seconds, V-Merang fires a charged shot that deals bonus damage and binds for 1 second.
  - serveur : géré (FLAG)
- [ ] **Sort 1 — Arcblade** (recharge 2s, dégâts de base 470)
  - Fires a Ringblade in a circular trajectory that returns to V-Merang. Can strike enemies multiple times.
    - MOVE MOVE_PRESET_SEE_TARGET
    - `1440190010` GRAPH SPHERE LengthX=350 portée≈2000, dégâts, traverse
- [ ] **Sort 2 — Backspin Blade** (recharge 3s, dégâts de base 305)
  - Fires a wildly spinning Ringblade at a target area, damaging and launching struck enemies.
    - MOVE MOVE_PRESET_SEE_TARGET
    - `1440190020` NONETARGET BOX LengthX=100 LengthY=150 portée≈1440, dégâts, traverse
      - `1440190021` GRAPH BOX LengthX=130 LengthY=200 portée≈560, dégâts, traverse, statuts: FLY 1.5s
- [ ] **Sort 3 — Scatterblade** (recharge 2s, dégâts de base 370)
  - Throws a Ringblade that bounces to nearby targets before returning. All targets in the path of the bouncing blade are struck.
    - `1440190030` HOMING SPHERE LengthX=250 portée≈2000, dégâts, traverse
- [ ] **Sort 4 — Ringblade Barrier** (recharge 2s, dégâts de base 80)
  - Throws a spinning Ringblade to a fixed position. The blade spins in place for {skill:180190040:remote:lifetime}, inflicting damage to enemies and reflecting enemy projectiles. Strikes over Ringblade Barrier's duration do not restore Ringblades. When Spinning Ringblade is used, the previous ring will
    - `1440190040` NONETARGET SPHERE LengthX=250 portée≈2200, sans dégâts, traverse
    - `1440190041` NONETARGET SPHERE LengthX=400, dégâts, traverse
    - `1440190043` NONETARGET SPHERE LengthX=250 portée≈2200, sans dégâts, traverse
    - `1440190044` NONETARGET SPHERE LengthX=400, dégâts, traverse
    - `1440190045` NONETARGET SPHERE LengthX=250 portée≈2200, sans dégâts, traverse
    - `1440190046` NONETARGET SPHERE LengthX=400, dégâts, traverse
    - MOVE MOVE_PRESET_SEE_TARGET
- [ ] **Ultime — Blade Storm** (recharge 30s, dégâts de base 730)
  - Flings a large tornado in a line, knocking enemies into the air and dealing light damage over 200m. Damage is reduced by 40% until the tornado is 8m away.
    - `1440190050` NONETARGET BOX LengthX=500 LengthY=500 portée=15000, dégâts, traverse, statuts: AIRBORNE 1.5s
    - statut sur soi/cible : GODMODE 6.0s
- [ ] **Esquive — Acrobatics** (recharge 0.3s)
  - Tumbles 8m towards your cursor. While moving, you are immune to enemy crowd control attacks.
    - statut sur soi/cible : SUPERARMOR 0.5s
    - GRAPH_MOVE_HORZ 800

<details><summary>Guide</summary>

- Passive Sametan Warrior: stores 2 Ringblades, 1 consumed per skill. Weapon attack: bonus damage every 8 s, binds target 1 s.
- Arcblade (A): Ringblade along a circular path that returns to her, "bends back to V-Merang, even if she moves after using this skill". Multi-target, long range.
- Backspin Blade (E): Ringblade at a set area, struck target becomes airborne. Very short range.
  In-game KR description: travels a fixed distance, then rises into the air and launches enemies (gameplay video confirms it pierces).
- Scatterblade: targets the enemy near the cursor hitbox, homing ("hits your target even if it moves"), bounces to another nearby target on hit.
- Ringblade Barrier: spinning Ringblade at a fixed area for a duration, damages and reflects enemy projectiles only.
- Ultimate Blade Storm: tornado in a straight line, 200 m (20000 units?), wide, fast, launches struck targets; damage -40% until the tornado is 8 m from V-Merang.
- Acrobatics (dodge): 8 m toward the cursor, CC immune while moving.
</details>

## Bumaro (SLIME)

- [ ] **Passif — Conservation of Mass** 
  - Charging up weapon attacks increases Mass. Weapon attacks drain 10 Mass. [Conservation] When Bumaro is thin, movespeed is increased while weapon damage, skill power, and defense are decreased. [Mass] When Bumaro is massive, it is slowed, while weapon damage, skill power, and defense are increased.
  - serveur : **non géré** (SLIME)
- [ ] **Sort 2 — Acidic Burst** (recharge 7s, dégâts de base 400)
  - Bumaro alters its genetic makeup to spew acidic jelly in a {skill:180200020:remote:range} area around itself. Consumes 0/25/50 Mass when used depending on Mass available. Damage and range are increased based on Mass spent.
    - `1440200020` NONETARGET SPHERE LengthX=720, dégâts, traverse
    - statut sur soi/cible : STAT 1.5s
- [ ] **Sort 3 — Cell Division** (recharge 15s)
  - Bumaro stimulates the ooze colonies within it, recovering {skill:180200030:status:regen:add:health} HP and {skill:180200030:status:stat:add:rageregen} Mass every second for {skill:180200030:status:durationtime}. Cannot move while channeling. Increases basic defense by 380%. Able to cancel by committ
    - statut sur soi/cible : HEAL 4.0s
    - statut sur soi/cible : STAT 4.0s
    - statut sur soi/cible : STAT 3.0s
- [ ] **Sort 4 — Rubberize** (recharge 23s)
  - Bumaro changes its genetic makeup, increasing elasticity and reflecting damage for {skill:180200040:status:durationtime}. After the duration, Bumaro rebounds with repulsive force, knocking down enemies within 5m. Mass consumed determines damage reflected (0 is 15%, 25 is 20%, 50 is 25%). Movespeed d
    - statut sur soi/cible : STAT 4.0s
    - statut sur soi/cible : STAT 6.0s
    - statut sur soi/cible : STAT 1.5s
- [ ] **Esquive — Oozify** (recharge 0.3s)
  - Liquefies for {skill:180200002:status:durationtime}, becoming untargetable. Enemies standing on Bumaro are slowed.
    - `1440200000` NONETARGET SPHERE LengthX=800, sans dégâts, traverse, statuts: SLOW 0.2s
    - statut sur soi/cible : EVADE 2.0s
    - statut sur soi/cible : BURROW 1.93s **[type non géré]**
    - statut sur soi/cible : STAT 1.5s
    - MOVE MOVE_PRESET_MOVE_NONE
- [ ] **Sort 21 — Cling** (recharge 20s)
  - Clings to either an ally or enemy for {skill:180200010:status:durationtime}. Causes different effects based on whether Bumaro has more or less HP than its target. On Allies: Lower HP: Grants {skill:180200012:status:stat:mul:movespeed:1}% movespeed. Higher HP: Restores {skill:180200012:status:regen:a
    - statut sur soi/cible : EVADE 2.0s
    - MOVE MOVE_PRESET_GOTO_CURRENT_TARGET
    - `1440200010` TARGET SPHERE LengthX=200 portée=2500, dégâts, s’arrête au 1er, statuts: CASTER 6.2s **[type non géré]**, FLAG 6.0s, alliés: CASTER 6.2s **[type non géré]**, FLAG 6.0s

<details><summary>Guide</summary>

- Weapon: slow long-range blob creating a damaging area where it lands; charging adds Mass.
- Cling: attach to an ally/enemy; enemy lower HP: drain, higher HP: slow; ally lower HP: heal, higher: speed.
- Acidic Burst: AoE around him, more Mass spent = more damage and radius.
- Cell Division: channel restoring HP and Mass, more defense, no CC immunity.
- Rubberize: reflects damage (scales with Mass); after 4 s knocks down enemies within 5 m.
- Passive: low Mass = speed, high Mass = damage/skill power/defense.
- Oozify: liquid 2 s, untargetable, enemies on him slowed.
- Ultimate Mitosis: 3 copies imitating skills (not Cling) and attacks.
</details>

## Mondo Zax (MONDOZAX)

- [ ] **Passif — Hyper-Charged** 
  - [Hyper-Charged] After each skill use, Mondo's next attack within 5 seconds deals additional damage. Lasts 1 attack.
  - serveur : **non géré** (aucune donnée)
- [ ] **Esquive — Chua Barrel Roll** (recharge 0.3s)
  - Initiate an evasive maneuver to quickly rocket 9m away. Cannot be targeted for {skill:180210002:status:durationtime}.
    - GRAPH_MOVE_HORZ 900
    - statut sur soi/cible : EVADE 0.3s
- [ ] **Sort 20 — Orbital Laser** (recharge 30s, dégâts de base 520)
  - Hacks into the Dredgion's laser system and causes it to track Mondo's Bolt Caster reticle. The beam slowly follows your cursor and deals heavy damage.
    - `1440210060` CASTER_TARGET SPHERE LengthX=300 portée≈3300, dégâts, traverse

<details><summary>Guide</summary>

- Prism Bot: cloaking field 1.5 s for everyone in the area; periodic +10% weapon dmg 5 s.
- Bruiser Bot: taunts nearby enemies 3 s; enemies within 15 m of the bot -30% ms; bot hits slow 1 s.
- Artillery Bot: 4 mortar shells on an area (320); the bot follows him and fires mortars periodically.
- Repair Bot: moves to an area, 250 shield to nearby allies 5 s; periodic 100 shield on him.
- Ultimate Orbital Laser: 680 dmg, 5 s cast time, slowly tracks the cursor.
- Passive: next attack within 5 s after a skill deals bonus damage. Survival: rockets away, untargetable.
- A new bot destroys the current one; enemies can destroy bots.
</details>

## Lilu (LILU)

- [ ] **Passif — Razor Beak** 
  - [Razor Beak] Every 4th weapon attack delivers an additional weapon strike.
  - serveur : géré (HIT_PLAGUE)
- [ ] **Sort 2 — Talon Swarm** (recharge 15s, dégâts de base 355)
  - Calls down an eagle strike every {skill:180220020:leveltext:012345:2:6:1.2} sec in the marked zone for {skill:180220020:remote:lifetime}, damaging all present enemies. Grants visibility within the marked area in Titan Ruins. Detects stealth in a radius of 5m.
    - `1440220020` NONETARGET SPHERE LengthX=800, sans dégâts, traverse, statuts: STAT 2.0s, BUSH_REVEAL 3.0s **[type non géré]**
      - `1440220021` NONETARGET SPHERE LengthX=800, dégâts, traverse
    - ENTITY 100034003
- [ ] **Sort 3 — Birds of a Feather** (recharge 17s)
  - Lilu soars into the air with some help from a feathered friend, traveling up to 25m and gaining CC immunity. [Combo] Recast to cancel.
    - `1440220030` NONETARGET BOX LengthX=300 LengthY=300, sans dégâts, traverse
    - statut sur soi/cible : SUPERARMOR 2.0s
    - statut sur soi/cible : STAT 1.5s
    - GRAPH_MOVE_HORZ 2500
- [ ] **Sort 4 — Feather Crest** (recharge 12s, dégâts de base 100)
  - Marks an enemy with a charm, revealing them for 3 seconds. Your Eagle can combo to attack the marked target. [Pecking Order] Haru barrels through all opposition to strike the marked target, stunning all struck enemies for {skill:180220040:status:durationtime}. Pecking Order costs 50MP to combo.
    - `1440220040` NONETARGET BOX LengthX=30 LengthY=350 portée=1400, dégâts, s’arrête au 1er, statuts: STAT 5.0s, BUSH_REVEAL 5.0s **[type non géré]**
- [ ] **Ultime — Screeching Crest** (recharge 30s, dégâts de base 535)
  - Marks the target with a screeching crest for {skill:180220050:remote:lifetime}. The crest emits powerful sonic pulses, dealing damage to nearby enemies every 1.5 seconds. Lilu can mark any target except herself. The mark is removed if the target dies or tags out.
    - `1440220050` NONETARGET SPHERE LengthX=200, sans dégâts, traverse
      - `1440220051` NONETARGET SPHERE LengthX=1000, dégâts, traverse
- [ ] **Esquive — Wingrider** (recharge 0.3s)
  - Lilu takes to the skies and evades incoming attacks.
    - statut sur soi/cible : EVADE 2.0s
    - MOVE MOVE_PRESET_MOVE_NONE
- [ ] **Sort 16 — Bird of Prey** (recharge 20s, dégâts de base 735)
  - Sends Haru to a target. Haru lifts the target into the air for {skill:180220010:status:durationtime:1}. Recasting will initiate an Eagle Slam. [Eagle Slam] Slams the target down in a new target area, causing a damaging shockwave.
    - ENTITY 100034000
    - CALL_CHILD_SKILL MONSTER_SKILL_ID_1
    - statut sur soi/cible : ACTIONBLOCK 3.0s

<details><summary>Guide</summary>

- Passive Razor Beak: every 4th attack deals extra damage.
- Bird of Prey: hawk lifts an enemy (or ally) and drops it at a chosen spot; airborne target can't attack/dodge/cast.
- Talon Swarm: AoE with damage over time, reveals area and stealth, faster neutral kills.
- Birds of a Feather: mobility, CC immune.
- Feather Crest: line strike marking the first target (revealed 3 s); the eagle flies to it, stunning the target and enemies on the way.
- Wingrider (survival): lifted into the air, dodges attacks.
- Ultimate Screeching Crest: mark dealing damage over time to the target and around it; removed if the target tags out.
</details>

## Ignuma (EFREET)

- [ ] **Passif — Incinerate** 
  - [Incineration] Damaging skills cause Burning on their targets for 5 seconds. Burning stacks up to 5 times. [Heart of Fire] Kindling is not lost over time when out of combat.
  - serveur : **non géré** (EFREET)
- [ ] **Sort 1 — Pyrobarrier** (recharge 6s, dégâts de base 45)
  - Block the next {skill:180230010:status:shiledcount} attack to hit you. Upon extinguishing your block, teleport forward and release a damaging explosion behind you that inflicts burning.
    - statut sur soi/cible : SHIELD 4.0s
    - statut sur soi/cible : SUPERARMOR 4.1s
- [ ] **Sort 3 — Fiery Advance** (recharge 10s, dégâts de base 420)
  - Charges forward and releases an explosion upon impact, damaging and knocking back any surrounding enemies.
    - GRAPH_MOVE_HORZ 900
    - `1440230030` NONETARGET BOX LengthX=300 LengthY=150, sans dégâts, s’arrête au 1er
- [ ] **Sort 4 — Elemental Wrath** (recharge 15s)
  - Hurls a flaming orb at the target, terrorizing them for {skill:180230040:status:durationtime} and inflicting burning on nearby enemies.
    - `1440230060` TARGET SPHERE LengthX=100 portée≈1500, sans dégâts, s’arrête au 1er, statuts: FEAR 1.5s
- [ ] **Ultime — Stoke the Coals** (recharge 30s)
  - Absorbs Kindling to instantly recover {skill:180230050:status:regen:mul:health}% max HP. Each burning target within your vicinity adds 100 HP to this healing effect, up to a maximum of 700.
    - statut sur soi/cible : HEAL 0.1s
    - `1440230050` NONETARGET SPHERE LengthX=1500, sans dégâts, traverse, statuts: EFFECT 0.3s **[type non géré]**
- [ ] **Esquive — Molten Shield** (recharge 0.3s)
  - Raises a shield and enters a defensive stance for 4 seconds. While the shield is raised, it blocks projectile attacks from the front and nullifies crowd control effects. Increases movespeed by 15% for 2 seconds. Shield is raised in the direction of your cursor and cannot turn. Skill use and attacks 
    - statut sur soi/cible : SUPERARMOR 5.0s
    - statut sur soi/cible : STAT 2.0s
    - MOVE MOVE_PRESET_SEE_TARGET
    - `1440230010` NONETARGET BOX LengthX=400 LengthY=150, sans dégâts, traverse
- [ ] **Esquive — Molten Shield: Cancel** 
  - Cancels Molten Shield.
    - MOVE MOVE_PRESET_SEE_TARGET

<details><summary>Guide</summary>

- Pyrobarrier: blocks the next attack; on block teleports forward with an explosion behind him.
- Set Ablaze (toggle): burning area following him, periodic low damage.
- Fiery Advance: charge; colliding with an enemy explodes, knocking back enemies around.
- Elemental Wrath: flaming orb at a target, fear 1.5 s, fire damage around.
- Ultimate Stoke the Coals: heal 20% HP + 100 per burning target in range, max 700.
- Passive Incinerate: skill hits burn 5 s (heal on hit), up to 4 stacks; the 4th deals extra damage and clears the burn.
- Molten Shield (survival): 4 s frontal projectile block, CC immune, +15% ms 2 s, can move, no skills/attacks, locked facing.
</details>

## Dr. Raoul (SHADOWHUNTER)

- [ ] **Passif — Synd Ingenuity** 
  - [Shadow Hunter] Landing hits with skills grants 2.5 sec of stealth. Slowed enemies take additional damage from weapon attacks. [Charged Claws] The first weapon attack after using a skill leaves a lightning rift for 2 sec, dealing periodic damage to enemies touching it.
  - serveur : **non géré** (aucune donnée)
- [ ] **Sort 1 — Ripper Beam** (recharge 8s, dégâts de base 610)
  - Fires a beam that follows your mouse cursor for {skill:180240010:remote:lifetime}, dealing heavy damage. Unable to move while firing. Attempting other actions will cancel Ripper Beam. Ripper Beam will only activate stealth on the last tick of damage.
    - statut sur soi/cible : STAT 10.0s
    - `1440240011` BEAM BEAM LengthX=20 portée=1300, dégâts, traverse
    - `1440240012` NONETARGET BOX LengthX=30 LengthY=100 portée=1600, sans dégâts, s’arrête au 1er
- [ ] **Sort 2 — Tesla Mines** (recharge 10s, dégâts de base 350)
  - Deploy an array of damaging mines in a random pattern. Mine explosions cause a {skill:180240020:status:stat:mul:movespeed}% slow on enemies for {skill:180240020:status:durationtime}.
    - statut sur soi/cible : STAT 10.0s
    - `1440240021` NONETARGET SPHERE LengthX=300, sans dégâts, s’arrête au 1er, statuts: BUSH_REVEAL 3.0s **[type non géré]**
      - `1440240023` NONETARGET SPHERE LengthX=300, dégâts, traverse, statuts: SLOW 2.0s
- [ ] **Sort 3 — Optic Disruptor** (recharge 6s)
  - Unleashes an eletrical blast around Raoul that paralyzes enemy optic nerves and halts visual signals to the brain for {skill:180240030:status:durationtime}. Non-Master enemies are terrorized instead of blinded.
    - statut sur soi/cible : STAT 10.0s
    - `1440240030` NONETARGET SPHERE LengthX=1500, dégâts, traverse, statuts: FLASH_HIT 1.0s **[type non géré]**
- [ ] **Sort 4 — Tele-Strike** (recharge 5s)
  - Teleports behind your target, evading damage. Your power cells retain some of the energy from the teleportation, increasing your attack speed by {skill:180240040:status:firedelay}% for {skill:180240040:status:durationtime}.
    - MOVE MOVE_PRESET_GOTO_CURRENT_TARGET_THROUGH
    - statut sur soi/cible : HIDE 2.5s **[type non géré]**
    - statut sur soi/cible : STAT 10.0s
    - statut sur soi/cible : EVADE 2.0s
    - statut sur soi/cible : STAT 1.0s
    - statut sur soi/cible : FIREDELAY 1.5s **[type non géré]**
- [ ] **Ultime — Shadow Drone Assault** (recharge 30s, dégâts de base 1055)
  - Dr. Raoul goes into Stealth Mode for 3 sec, and creates a Shadow Drone to take his place, while marking an area to be assaulted by Shadow Drones. All targets in the area are slowed by {skill:180240050:status:stat:mul:movespeed}% for {skill:180240050:status:durationtime}. After a short delay, several
    - ENTITY 100034010
    - `1440240050` NONETARGET SPHERE LengthX=1200, sans dégâts, traverse, statuts: SLOW 1.0s
    - statut sur soi/cible : HIDE 2.5s **[type non géré]**
    - statut sur soi/cible : EVADE 2.0s
    - statut sur soi/cible : STAT 10.0s
- [ ] **Esquive — Shadow Step** (recharge 0.3s)
  - Instantly dashes 4.5m.
    - MOVE MOVE_PRESET_WARP

<details><summary>Guide</summary>

- Ripper Beam: beam following the cursor 4 s, up to 610 dmg, can't move, any action cancels, stealth on the last tick.
- Tesla Mines: random pattern, 170 dmg, slow 50% 2 s, 55 EP.
- Optic Disruptor: blast around him, blind 1 s (non-masters feared), no damage.
- Tele-Strike: teleports behind the target, +50% attack speed 2 s, stealth.
- Ultimate Shadow Drone Assault: stealth 3 s, drone left in place, marked area slows 50% 2 s, drones dive after a delay; recast teleports to the area.
- Passive: landing a skill = 1 s stealth; slowed enemies take more weapon damage. Shadow Step: 4.5 m.
- Resource EP: faster regen, lower max than MP.
</details>

## Rytlock (RYTLOCK)

- [ ] **Passif — Warband Champion** 
  - [Tenacious] Each time you are stuck by crowd control you will gain a buff that renders you immune to the next crowd control effect. [Sohothin] The first and second hits of your weapon attack chain cause burning, while the third attack deals 20% additional damage.
  - serveur : **non géré** (UNSHAKABLE)
- [ ] **Sort 1 — Brutish Leap** (recharge 16s, dégâts de base 280)
  - Leaps to a target area, delivering a crippling slash that slows enemies by {skill:180250010:status:stat:mul:movespeed}% for {skill:180250010:status:durationtime}.
    - MOVE MOVE_PRESET_TARGET_POS_LEAP
    - GRAPH_MOVE_HORZ 1200
    - `1440250011` NONETARGET SPHERE LengthX=520, dégâts, traverse, statuts: SLOW 1.5s
- [ ] **Sort 2 — Hamstring** (recharge 12s, dégâts de base 700)
  - Slashes at enemies in front of you, dealing heavy damage and slowing them by {skill:180250020:status:stat:mul:movespeed}% for {skill:180250020:status:durationtime}.
    - `1440250020` NONETARGET BOX LengthX=450 LengthY=400, dégâts, s’arrête au 1er, statuts: SLOW 2.0s
- [ ] **Sort 3 — Head Shot** (recharge 12s, dégâts de base 190)
  - Fires a shot that rattles your target's skull, damaging and silencing them for {skill:180250030:status:durationtime}.
    - `1440250030` TARGET SPHERE LengthX=200 portée≈3000, dégâts, s’arrête au 1er, statuts: SILENCE 2.5s **[type non géré]**, STAT 1.0s
- [ ] **Sort 4 — Pistol Whip** (recharge 18s, dégâts de base 270)
  - Swipes targets in a cone, stunning them for {skill:180250040:status:durationtime}. During the stun, you unleash a flurry of slashes, dealing heavy damage in a cone. Movement and attacks can cancel this ability early.
    - `1440250040` NONETARGET BOX LengthX=450 LengthY=300, dégâts, traverse, statuts: STUN 1.0s
    - `1440250041` NONETARGET BOX LengthX=450 LengthY=300, dégâts, traverse
    - `1440250042` NONETARGET BOX LengthX=450 LengthY=300, dégâts, traverse
- [ ] **Esquive — Combat Roll** (recharge 0.3s)
  - Quickly rolls 6m. During a roll, you are immune to enemy attacks for {skill:180250002:status:durationtime}.
    - statut sur soi/cible : EVADE 0.35s
    - GRAPH_MOVE_HORZ 600
    - MOVE MOVE_PRESET_MOVE_NONE

<details><summary>Guide</summary>

- Passive: 1st and 2nd weapon attacks burn; hitting a burning target +20% damage.
- Tenacious (survival): immune to the next CC after being struck by CC.
- Brutish Leap: leap to an area, 280 dmg, slow 25% 1.5 s, cd 16 s, range 10 m.
- Hamstring: damage after a short delay, 700 dmg, slow 30% 2 s, cd 12 s, short range.
- Head Shot: ranged 190 dmg, 12 m, short silence.
- Pistol Whip: swipe stun 1.5 s then 5 strikes (270 total), cd 18 s, any action cancels it.
- Ultimate Sohothin's Wrath: weapon attacks become long-range piercing for 10 s or 6 attacks, burn 4 s.
</details>

## Kat the Cat (CATTHECAT)

- [ ] **Passif — Feline Grace** 
  - [Feline Grace] Increases attack speed by 8% for 5 seconds each time Kat activates a skill.
  - serveur : **non géré** (aucune donnée)
- [ ] **Ultime — Pounce!** (recharge 15s)
  - Pounces onto an ally or enemy, knocking back enemies in your path and slowing hostile targets by 50% for 0.75 seconds. May be used twice before cooling down, and on the second cast Pounce! will knock down hostile targets. On Allies: Removes 1 debuff and increases their movespeed by {skill:180260050:
    - MOVE MOVE_PRESET_GOTO_CURRENT_TARGET
    - `1440260050` NONETARGET BOX LengthX=450 LengthY=400, sans dégâts, traverse, statuts: PUSH 0.5s
    - `1440260051` TARGET SPHERE LengthX=100 portée≈1750, sans dégâts, s’arrête au 1er, statuts: STAT 3.0s, DISPEL 0.1s
    - `1440260054` TARGET SPHERE LengthX=100 portée≈1750, sans dégâts, s’arrête au 1er, statuts: STAT 3.0s, DISPEL 0.1s, SLOW 0.75s
    - statut sur soi/cible : FIREDELAY 5.0s **[type non géré]**
- [ ] **Esquive — If I Fits, I Sits** (recharge 0.3s)
  - Grants invincibility for {skill:180260002:status:durationtime}. You cannot move while in your box. Gain +50% movespeed for a short time once invincibility ends.
    - statut sur soi/cible : GODMODE 1.2s

<details><summary>Guide</summary>

- Passive: +8% attack speed for 5 s per skill used.
- Mew: stuns around 0.5 s; stance: long-range weapon attacks for 15 s or 7 attacks.
- Kai: reflects 50% damage for 0.8 s; stance: short swipes, +10% move speed and crit, swipes refresh the reflect.
- King: binds around 1 s; stance: splash attacks, slow 15% 1.2 s (2 stacks), +15% defense, 10 HP regen.
- Queen: stance: mid-range fans, allies/enemies hit get +10% damage and +10% defense 10 s.
- Ultimate Pounce: jumps onto an ally/enemy (2 charges), knocks back enemies on the way, knocks down the target;
  ally: -1 debuff, +10% ms 3 s, 100 HP over 3 s; enemy: dispel 1 buff, -10% weapon dmg 3 s, Kat +10% weapon dmg.
- If I Fits, I Sits: invincible 1.2 s, can't move, then +50% move speed briefly.
</details>

## R&B (RNB)

- [ ] **Passif — Vacuum Pulse** 
  - [Vacuum Pulse] Every 8 seconds, your next weapon attack slows by 30% for 2 seconds.
  - serveur : géré (FLAG)
- [ ] **Sort 1 — Ram's Well** (recharge 12s, dégâts de base 260)
  - After a short delay, Ram's Well will implode and deal damage to all enemies caught within it. Damaged enemies are dragged to the center of the well.
    - `1440270011` NONETARGET SPHERE LengthX=800, dégâts, traverse, statuts: DRAGGEDBYFORCE 1.0s **[type non géré]**
- [ ] **Sort 2 — Bacchus's Fist** (recharge 2s, dégâts de base 755)
  - Bacchus slams his arms into the ground, reshaping them into spears and skewering enemies at a distance for heavy damage.
    - `1440270021` NONETARGET BOX LengthX=300 LengthY=120, dégâts, traverse
- [ ] **Sort 3 — Bacchus's Retaliation** (recharge 5s)
  - On activation, grants a {skill:180270030:status:durationtime:1} window where Bacchus will nullify the next 1 attack from a Master. Melee attackers are struck by a retaliatory strike, and stunned for {skill:180270030:status:durationtime:2}. Lasts for 1 defense and does not apply to ranged attacks or 
    - statut sur soi/cible : SHIELD 1.0s
    - statut sur soi/cible : COUNTERSTATUS 1.0s **[type non géré]**
- [ ] **Sort 4 — Ram's Impaler** (recharge 10s, dégâts de base 620)
  - Launches Ram's tail, hooking into the first target in a line. Ram pulls himself to the hooked target, dealing moderate damage and binding them.
    - `1440270040` NONETARGET BOX LengthX=90 LengthY=50 portée=900, dégâts, s’arrête au 1er, statuts: FLAG 0.5s
- [ ] **Ultime — Duality Slam** (recharge 60s)
  - [Ram's Wrath] After a casting time of {skill:180270050:status:durationtime:1} Ram releases a blast of power and binds all enemy Masters on the battlefield for {skill:180270050:status:durationtime}. Masters can swap out of the debuff before it binds them to dispel it. [Bacchus's Recovery] Heals {skil
    - statut sur soi/cible : HEAL 0.5s
- [ ] **Esquive — Shift** (recharge 0.3s)
  - Shift your position 7m. Grants weapon attack immunity for {skill:180270002:status:durationtime}.
    - statut sur soi/cible : EVADE 0.35s
    - GRAPH_MOVE_HORZ 700

<details><summary>Guide</summary>

- Weapon: targets an area, AoE, no overheat, interval between shots.
- Ram's Well: vacuum damaging and pulling enemies to the center, small delay, wide.
- Bacchus's Fist: small area, quick, big damage.
- Bacchus's Retaliation: 1 s window returning one melee attack, stuns the attacker 1 s.
- Ram's Impaler: line attack moving R&B behind the target (no dash on miss).
- Ultimate Duality Slam: 3.5 s cast, invincible, binds ALL enemies on the map 2 s, +20% HP and 300 MP.
- Vacuum Pulse: next weapon attack slows 30% 2 s, cd 8 s. Shift (survival): 7 m leap, weapon immunity 0.3 s.
</details>

## Sonid (BATTER)

- [ ] **Passif — Underdog** 
  - [Underdog] For every 10% max health missing, Sonid gains additional critical strike damage.
  - serveur : **non géré** (BATTER)
- [ ] **Sort 1 — Scatterball** (recharge 12s, dégâts de base 600)
  - Knocks {skill:180280010:leveltext:012345:4:6:6} balls out in a cone. Multiple balls can hit the same target, but deal reduced damage.
    - `1440280010` GRAPH SPHERE LengthX=100 portée=1200, dégâts, s’arrête au 1er
    - `1440280014` GRAPH SPHERE LengthX=100 portée=1200, dégâts, s’arrête au 1er
    - `1440280011` GRAPH SPHERE LengthX=100 portée=1200, dégâts, s’arrête au 1er
    - MOVE MOVE_PRESET_MOVE_NONE
- [ ] **Sort 3 — Power Slide** (recharge 14s, dégâts de base 500)
  - Bounds to the target area, finishing in a slide that knocks enemies 2m away and deals moderate damage.
    - MOVE MOVE_PRESET_TARGET_POS
    - statut sur soi/cible : SUPERARMOR 1.0s
    - GRAPH_MOVE_HORZ 500
- [ ] **Sort 4 — Firecracker** (recharge 14s, dégâts de base 550)
  - Strikes a ball so hard that it bursts into flames, damaging all enemies in a line after a short delay. A line of fire trails the ball for {skill:180280040:status:durationtime} damaging all enemies who pass through it.
    - `1440280080` NONETARGET BOX LengthX=180 LengthY=1450, sans dégâts, traverse
    - MOVE MOVE_PRESET_MOVE_NONE
    - `1440280040` NONETARGET BOX LengthX=100 LengthY=1400, dégâts, traverse
    - `1440280041` NONETARGET BOX LengthX=150 LengthY=1400, sans dégâts, traverse, statuts: BURNING 3.0s
- [ ] **Esquive — Dustkicker** (recharge 0.3s)
  - Slides 6m and creates a dust cloud. Within the dust cloud, all friendly and hostile weapon attacks are nullified.
    - `1440280001` NONETARGET BOX LengthX=200 LengthY=400, sans dégâts, traverse, statuts: EVADE 1.0s, alliés: EVADE 1.0s
    - GRAPH_MOVE_HORZ 600
- [ ] **Sort 16 — Catcher's Mitt** (recharge 10s, dégâts de base 825)
  - Dashes in the target direction with a huge glove, blocking enemy projectiles. [Fastball] Pitch a damaging ball in the target direction.
    - `1440280020` NONETARGET BOX LengthX=400 LengthY=200, sans dégâts, traverse
    - statut sur soi/cible : FLAG 2.0s
    - GRAPH_MOVE_HORZ 700
- [ ] **Sort 19 — Knock Em Out of the Park** (recharge 30s, dégâts de base 790)
  - Marks a target to be knocked out, quickly moving to their position. Sonid strikes the target with a launching blow, sending them flying for 12m. Also launches allies.
    - statut sur soi/cible : EVADE 1.0s
    - MOVE MOVE_PRESET_GOTO_CURRENT_TARGET_CHASE
    - GRAPH_MOVE_HORZ 1200
    - `1440280050` NONETARGET BOX LengthX=300 LengthY=350, dégâts, traverse, statuts: FLY 1.95s, alliés: FLY 1.95s

<details><summary>Guide</summary>

- Scatterball: 4 balls in a cone (6 at level 6), several can hit one target with reduced damage.
- Catcher's Mitt: dash blocking a projectile; recast throws a damaging ball.
- Power Slide: slide to an area, pushes the FIRST enemy hit back 2 m (knockdown at level 6).
- Firecracker: ball at the target leaving a fire line, damage for 3 s in it.
- Passive Underdog: +5% crit per 10% missing HP.
- Dustkicker (survival): slide + dust cloud nullifying all weapon attacks inside.
- Ultimate: launches the target (ally or enemy) and masters near it 12 m into the air.
- Weapon: charged, more damage and projectile speed.
</details>

## Merope (ANDROA)

- [ ] **Passif — Pleiades Michani Enas** 
  - [Overdrive] Increases attack speed by 20% for 5 sec when skills are used.
  - serveur : **non géré** (aucune donnée)
- [ ] **Sort 2 — Devastator Rocket** (recharge 10s, dégâts de base 445)
  - Fires a low-speed missile that explodes upon reaching the targeted area. Flies {skill:180290020:remote:range}, dealing damage to enemies and terrain it passes through, as well as when it explodes in an area of {skill:180290020:remote:range:1} at the end of its flight path.
    - GRAPH_MOVE_HORZ -180
    - `1440290020` NONETARGET BOX LengthX=90 LengthY=48 portée≈1600, sans dégâts, traverse
      - `1440290022` NONETARGET SPHERE LengthX=800, dégâts, traverse
      - `1440290021` NONETARGET SPHERE LengthX=400, dégâts, traverse
    - statut sur soi/cible : FIREDELAY 5.0s **[type non géré]**
- [ ] **Sort 3 — Vector Strike** (recharge 12s, dégâts de base 550)
  - Quickly accelerates 9m in a line. Slams into the first enemy in your path, dealing damage and knocking them away. Merope will rocket off the enemy, retreating backwards before landing.
    - statut sur soi/cible : FIREDELAY 5.0s **[type non géré]**
    - `1440290030` NONETARGET BOX LengthX=250 LengthY=80, dégâts, traverse, statuts: PUSH 0.5s
    - GRAPH_MOVE_HORZ 900
- [ ] **Sort 4 — Conversion Battery** (recharge 15s)
  - Creates an energy field that can absorb damage. The energy field nullifies {skill:180290040:status:shiledcount} strikes of damage for {skill:180290040:status:durationtime}. When the shield breaks after {skill:180290040:status:shiledcount} attacks, {skill:180290040:status:regen:add:mana:1} MP and {sk
    - statut sur soi/cible : SHIELD 5.0s
    - statut sur soi/cible : FIREDELAY 5.0s **[type non géré]**
- [ ] **Ultime — Target Locked** (recharge 30s, dégâts de base 850)
  - Scans the area, locking on up to 4 targets before firing missiles at them. Fires {skill:180290050:leveltext:012:4:345:6:6:8} homing missiles at locked on targets. If there less than 4 targets in range, multiple missiles will strike the same target. Multiple missing striking the same target have thei
    - statut sur soi/cible : FIREDELAY 5.0s **[type non géré]**
    - statut sur soi/cible : SUPERARMOR 1.0s
    - statut sur soi/cible : ACTIONBLOCK 2.0s
    - `1440290050` FOLLOW_TARGET BOX LengthX=90 LengthY=48 portée≈280, dégâts, s’arrête au 1er
- [ ] **Esquive — Boost to Get Through!** (recharge 0.3s)
  - Uses a booster to quickly move over a distance of 7m.
    - GRAPH_MOVE_HORZ 700
- [ ] **Sort 16 — Missile Salvo** (recharge 10s, dégâts de base 795)
  - Designates the starting point of your salvo line of {skill:180290010:leveltext:012345:4:6:6} missiles. A second reticle will appear to designate the direction of your salvo line. [Combo] Recast to fire the salvo for heavy damage.
    - MOVE MOVE_PRESET_MOVE_NONE
    - statut sur soi/cible : FIREDELAY 5.0s **[type non géré]**
    - ENTITY 100041050

<details><summary>Guide</summary>

- Missile Salvo: click-drag to set start point and direction of a missile line, 12 m, hits behind walls, 795 dmg.
- Devastator Rocket: slow missile damaging along its path, explodes at the target area.
- Vector Strike: accelerates in a straight line, STOPS on the first enemy hit, 715 dmg, knockback; also a dash.
- Conversion Battery: 200 MP, energy field 5 s nullifying up to 5 attacks; after 5: +200 MP and 10% max HP.
- Ultimate Target Locked: homing missiles in a fan, up to 4 targets (fewer targets = several missiles each).
- Boost (survival): 7 m. Passive: weapon heat reduced after a skill cast; +10% weapon damage with another android tagged.
</details>

## Maia (ANDROB)

- [ ] **Passif — Pleiades Michani Dyo** 
  - [Rejuvenate] Restores 250 HP to the lowest health Master within 8m on skill use.
  - serveur : **non géré** (aucune donnée)
- [ ] **Sort 1 — Savior Protocol** (recharge 16s)
  - Flies to and picks up an allied target. Maia shields the protected target for {skill:180300010:status:durationtime}, absorbing {skill:180300010:status:stat:add:shield_defence} damage. [Savior's Retreat] Dashes a moderate distance, dropping off the protected Master at the end of the dash. Shield pers
    - MOVE MOVE_PRESET_GOTO_CURRENT_TARGET
    - `1440300010` NONETARGET SPHERE LengthX=650, sans dégâts, s’arrête au 1er, statuts: TARGET 3.2s **[type non géré]**, SHIELD 2.0s
    - statut sur soi/cible : HEAL 0.5s
- [ ] **Sort 2 — Recall Tether** (recharge 15s)
  - Places a recall pad that is tethered to Maia at the targeted area for {skill:180300020:remote:lifetime}. Allies who step on the pad are pulled towards Maia. Maia's gains {skill:180300020:status:stat:mul:movespeed:1}% movespeed for {skill:180300020:status:durationtime:1} when she places the Recall Pa
    - ENTITY 100041040
    - `1440300020` NONETARGET SPHERE LengthX=400, sans dégâts, traverse, statuts: DRAGGED 0.4s, ACTIONBLOCK 0.4s
    - statut sur soi/cible : HEAL 0.5s
    - statut sur soi/cible : STAT 2.0s
- [ ] **Sort 4 — Anti-Kinesis Barrier** (recharge 15s)
  - Places a barrier that knocks enemies back when they touch it. Lasts for{skill:180300040:remote:lifetime}. Each target can only be knocked back once every 2 seconds. Barrier persists through tagging.
    - `1440300040` NONETARGET BOX LengthX=800 LengthY=50, sans dégâts, traverse, statuts: PUSH 0.7s, alliés: STAT 1.5s
    - statut sur soi/cible : HEAL 0.5s
- [ ] **Ultime — Synergy Protocol** (recharge 90s)
  - Maia's systems lock on to the lowest health allied Master. She teleports to them after a brief delay and makes them invincible for {skill:180300050:status:durationtime:1}.
    - statut sur soi/cible : SUPERARMOR 0.5s
    - statut sur soi/cible : FLAG 0.1s
- [ ] **Esquive — Boost to Get Through!** (recharge 0.3s)
  - Uses Maia's thrusters to quickly boost 7m.
    - GRAPH_MOVE_HORZ 700

<details><summary>Guide</summary>

- Combat Protocol: shield 2 s, more range/damage/attack speed.
- Recall Tether: recall pad 5 s; allies stepping on it are pulled to her; she gains +50% ms.
- Savior Protocol: flies to an ally, picks it up (shield), dashes and drops it.
- Anti-Kinesis Barrier: enemies touching it are knocked back (once per 2 s per target).
- Passive: after a skill, 150 HP to the most wounded master within 8 m.
- Ultimate Synergy Protocol: teleports to the lowest-HP ally anywhere, invincible 2 s, brief delay. Survival: 7 m dash.
</details>

## Tulam (ATTACKER)

- [ ] **Passif — Chiseled Resolve** 
  - When Tulam's health falls below 50%, his weapon damage is increased by 10%, and his defense is increased by 25%
  - serveur : **non géré** (HPTRIGGER)
- [ ] **Sort 1 — Knuckle Duster** (recharge 12s, dégâts de base 505)
  - Spin into a whirlwind attack that deals damage in a {skill:180310010:remote:range} radius and reflects projectiles. [Combo] Recast to cancel.
    - `1440310010` NONETARGET SPHERE LengthX=450, dégâts, traverse
- [ ] **Sort 2 — Landslide** (recharge 10s, dégâts de base 65)
  - Rushes to an enemy target and launches them into the air while flinging them behind you.
    - MOVE MOVE_PRESET_GOTO_CURRENT_TARGET_THROUGH_CHASE
    - `1440310020` NONETARGET BOX LengthX=400 LengthY=700, dégâts, s’arrête au 1er, statuts: FLY 1.8s, alliés: FORCEBACK 0.3s
    - `1440310022` NONETARGET BOX LengthX=220 LengthY=120, sans dégâts, traverse, statuts: FORCEBACK 0.1s
- [ ] **Sort 3 — Tunnel Strike** (recharge 18s, dégâts de base 75)
  - Tunnel through the ground to explode from a target area, avoiding incoming attacks and launching enemies into the air at your exit point.
    - MOVE MOVE_PRESET_TARGET_POS
    - GRAPH_MOVE_HORZ 1200
    - statut sur soi/cible : BURROW 3.0s **[type non géré]**
    - `1440310030` NONETARGET SPHERE LengthX=420, dégâts, traverse, statuts: AIRBORNE 1.5s
    - statut sur soi/cible : FLAG 3.0s
- [ ] **Sort 4 — Weight of Stone** (recharge 10s)
  - Lets out a bone-shaking shout to weaken all enemies within {skill:180310040:remote:range}. Reduces Weapon Damage and Skill Power by {skill:180310040:status:stat:mul:attack}% for {skill:180310040:status:durationtime}.
    - `1440310040` NONETARGET SPHERE LengthX=1200, sans dégâts, traverse, statuts: STAT 5.0s
- [ ] **Esquive — Stone Faced** (recharge 0.3s)
  - Tulam increases the density of his outer shell, increasing defense by {skill:180310002:status:stat:mul:defence}% for {skill:180310002:status:durationtime} and granting CC immunity. Movespeed increased by 15% for 2 seconds when using Stone Faced.
    - statut sur soi/cible : SUPERARMOR 2.5s
    - statut sur soi/cible : STAT 2.0s
    - MOVE MOVE_PRESET_MOVE_NONE
- [ ] **Sort 12 — Solquake** (recharge 30s, dégâts de base 475)
  - Tulam smashes his fists into the ground, letting loose a shockwave in a {skill:180310050:remote:range} radius that deals damage to all enemies and pulls them towards the center. The farther they are from the center, the more damage and air time is increased. Increases up to a max of 200% damage.
    - `1440310055` NONETARGET SPHERE LengthX=1500, sans dégâts, traverse
    - `1440310050` NONETARGET SPHERE LengthX=1500, dégâts, traverse, statuts: FLY 1.0s

<details><summary>Guide</summary>

- Knuckle Duster: whirlwind damaging around, reflects projectiles, can move slowly.
- Landslide: slides to a target and flings it behind him (ally or enemy).
- Tunnel Strike: underground (untargetable), bursts up launching enemies.
- Weight of Stone: around him, -weapon damage and skill power (stamina drain at level 6).
- Passive: below 50% HP more weapon damage and defense.
- Stone Faced (survival): defense + super armor 5 s.
- Ultimate Solquake: pulls enemies to him with damage and launches them; farther = longer air time.
</details>

## Pancuga (BOXER)

- [ ] **Passif — The Ol' One-Two** 
  - [Float like a Butterfly] Each time you hit an enemy with a skill, they are debuffed for 1.5 seconds and take 15% increased damage from your Weapon Attacks. [Sting like a Bee] Upon successfully dodging an attack, any retaliatory attack will stun for 0.5 sec. Lasts for 3 sec or 1 attack.
  - serveur : **non géré** (STATUS_ON_EVADE)
- [ ] **Esquive — Bob and Weave** (recharge 0.3s)
  - Dodges incoming attacks for {skill:180320002:status:durationtime}. Dodging grants 15% movespeed for 2 seconds.
    - statut sur soi/cible : EVADE 0.5s
    - statut sur soi/cible : STAT 2.0s
- [ ] **Sort 19 — Rocket Punch** (recharge 15s, dégâts de base 385)
  - Charges up Pancuga's Synd arm before launching a devastating punch in a line. Launches all enemies you collide with into the air. Distance covered ranges from 8m to 18m, and damage is increased up to 1.5x over 3 seconds of charging.
    - MOVE MOVE_PRESET_TARGET_POS
    - GRAPH_MOVE_HORZ 1800
    - `1440320040` NONETARGET BOX LengthX=220 LengthY=120, dégâts, traverse, statuts: FLY 1.8s, FLAG 1.5s
- [ ] **Sort 21 — Punch Out** (recharge 11s, dégâts de base 385)
  - Unleashes a swift set of punches in front of you. [Combination] Punch Out can be used multiple times in quick succession. - First Use: Fires out a twin punch combo. - Second Use: Fires out another twin punch combo. - Third Use: Fires out a punch combo, striking {skill:180320010:leveltext:012345:3:6:
    - `1440320010` NONETARGET BOX LengthX=200 LengthY=350, dégâts, traverse, statuts: FLAG 1.5s
- [ ] **Sort 22 — Southpaw Special** (recharge 10s, dégâts de base 440)
  - Unleashes a heavy-hitting hook combo. [Glass Jaw] Can be used multiple times in quick succession. - First Use: Throws a heavy left hook, dealing damage. - Second Use: Throw an uppercut, stunning and damaging struck enemies for {skill:180320020:status:durationtime}.
    - `1440320020` NONETARGET BOX LengthX=200 LengthY=350, dégâts, traverse, statuts: FLAG 1.5s
- [ ] **Sort 23 — Work the Body** (recharge 12s, dégâts de base 450)
  - Dashes forward and throws a series of punches. [Stick and Move] Can be used multiple times in quick succession. - First Use: Dashes forward and throws out a punch. - Second Use: Dashes forward while throwing out a punch that knocks targets back. - Third Use: Dashes forward and throws out a punch.
    - `1440320030` NONETARGET BOX LengthX=200 LengthY=350, dégâts, traverse, statuts: FLAG 1.5s
    - GRAPH_MOVE_HORZ 200
- [ ] **Sort 24 — Light Stepper** (recharge 9s)
  - Uses short dashes to evade incoming damage and reposition over {skill:180320040:status:durationtime}. [Twinkle Toes] Can be used multiple times in quick succession. - First Use: Dashes backwards, evading incoming attacks. - Second Use: Dashes forwards, evading incoming attacks.
    - statut sur soi/cible : EVADE 0.4s
    - GRAPH_MOVE_HORZ -200

<details><summary>Guide</summary>

- Punch Out: punch; recast twin punch; third cast triple punch.
- Southpaw Special: hook combo; recast uppercut stunning 0.5 s.
- Work the Body: dash + punches, twice; the second knocks back.
- Light Stepper: back dash (evade 0.7 s); quick recast dashes forward.
- Ultimate Rocket Punch: straight punch launching all enemies hit; charge up to 3 s = up to 18 m and 1.5x damage.
- Passives: +20% weapon attack per hit within 1.5 s after a skill; a successful dodge allows one counter (stun 0.5 s) within 3 s.
- Bob and Weave (survival): dodge 0.5 s, +15% ms 2 s. Consecutive skills trigger stronger combo attacks.
</details>

## Nanurunerk (SHUGOTRADER)

- [ ] **Passif — Savvy Shugo** 
  - [High Roller] Increases your RNG Meter by 10 when picking up Bundles. Once the RNG Meter is full, your next skill has an empowered effect. [Nyerk Off] Every sixth Weapon Attack deals 80% additional damage, grants 10 RNG, and slows for 1 sec.
  - serveur : **non géré** (aucune donnée)
- [ ] **Sort 1 — Enchantment Stone Bundle** (recharge 14s, dégâts de base 895)
  - Throws an Enchantment Stone Bundle. When picked up, this Bundle increases Weapon Damage by {skill:180330010:status:stat:mul:attack}% and Skill Power by 200 for {skill:180330010:status:durationtime}. [High Roller] Bundles deal damage when thrown at enemies and instead increases weapon damage by 35% a
    - `1440330010` GRAPH SPHERE LengthX=200 portée≈1500, sans dégâts, s’arrête au 1er
      - `1440330011` NONETARGET SPHERE LengthX=150, sans dégâts, s’arrête au 1er, statuts: STAT 10.0s, STAT 10.0s, BUSH_REVEAL 3.0s **[type non géré]**, alliés: STAT 10.0s, STAT 10.0s
- [ ] **Sort 2 — Supply Bundle** (recharge 10s, dégâts de base 720)
  - Throws a Supply Bundle that lasts for {skill:180330020:status:durationtime} and grants bonuses when picked up. The Bundle can be one of two options. Anti-Shock Scroll: Grants a shield that absorbs {skill:180330020:status:stat:add:shield_defence} damage. Courage Scroll: Increases Attack Speed by {ski
    - `1440330020` GRAPH SPHERE LengthX=200 portée≈1500, sans dégâts, s’arrête au 1er
      - `1440330021` NONETARGET SPHERE LengthX=150, sans dégâts, s’arrête au 1er, statuts: SHIELD 10.0s, BUSH_REVEAL 3.0s **[type non géré]**, alliés: SHIELD 10.0s
- [ ] **Sort 3 — Manastone Bundle** (recharge 14s)
  - Throws a Manastone Bundle. When picked up, this Bundle permanently increases weapon damage by {skill:180330030:status:stat:mul:attack}%, critical strike chance by {skill:180330030:status:stat:mul:attack}%, or skill power by 20, each stacking up to 10 times. Manastone Bundle effects persist through t
    - `1440330030` GRAPH SPHERE LengthX=200 portée≈1500, sans dégâts, s’arrête au 1er
      - `1440330031` NONETARGET SPHERE LengthX=150, sans dégâts, s’arrête au 1er, statuts: STAT, BUSH_REVEAL 3.0s **[type non géré]**, alliés: STAT
- [ ] **Sort 4 — Trickster Bundle** (recharge 12s, dégâts de base 100)
  - Throws a Trickster Bundle that grants {skill:180330040:status:stat:mul:movespeed}% Movespeed for {skill:180330040:status:durationtime} when picked up by allies, or binds for {skill:180330040:status:durationtime:1} when picked up by enemies. [High Roller] The Bundle deals damage when thrown at enemie
    - `1440330040` GRAPH SPHERE LengthX=200 portée≈1500, sans dégâts, s’arrête au 1er
      - `1440330041` NONETARGET SPHERE LengthX=150, sans dégâts, s’arrête au 1er, statuts: ROOT 2.0s, STAT 4.0s, BUSH_REVEAL 3.0s **[type non géré]**, alliés: STAT 3.0s, STAT 4.0s
- [ ] **Ultime — FIRESALE!** (recharge 30s, dégâts de base 940)
  - Throws a large bundle that knocks enemies back, away from the center, and shoots out a collection of bundles.
    - `1440330050` GRAPH SPHERE LengthX=50 portée≈1500, sans dégâts, s’arrête au 1er
      - `1440330061` NONETARGET SPHERE LengthX=800, dégâts, traverse, statuts: BUSH_REVEAL 3.0s **[type non géré]**, PUSH 0.8s
    - `1440330011` NONETARGET SPHERE LengthX=150, sans dégâts, s’arrête au 1er, statuts: STAT 10.0s, STAT 10.0s, BUSH_REVEAL 3.0s **[type non géré]**, alliés: STAT 10.0s, STAT 10.0s
    - `1440330021` NONETARGET SPHERE LengthX=150, sans dégâts, s’arrête au 1er, statuts: SHIELD 10.0s, BUSH_REVEAL 3.0s **[type non géré]**, alliés: SHIELD 10.0s
    - `1440330023` NONETARGET SPHERE LengthX=150, sans dégâts, s’arrête au 1er, statuts: FIREDELAY 10.0s **[type non géré]**, BUSH_REVEAL 3.0s **[type non géré]**, alliés: FIREDELAY 10.0s **[type non géré]**
    - `1440330031` NONETARGET SPHERE LengthX=150, sans dégâts, s’arrête au 1er, statuts: STAT, BUSH_REVEAL 3.0s **[type non géré]**, alliés: STAT
    - `1440330041` NONETARGET SPHERE LengthX=150, sans dégâts, s’arrête au 1er, statuts: ROOT 2.0s, STAT 4.0s, BUSH_REVEAL 3.0s **[type non géré]**, alliés: STAT 3.0s, STAT 4.0s
- [ ] **Esquive — Deal Chaser** (recharge 0.3s)
  - Increases movespeed by {skill:180330002:status:stat:mul:movespeed}% for {skill:180330002:status:durationtime}.
    - statut sur soi/cible : STAT 2.0s

<details><summary>Guide</summary>

- Passive: +10 RNG per weapon hit and bundle pickup; full meter empowers the next skill; every 7th attack +10 RNG, slow 1 s, more damage.
- Enchantment Stone Bundle: +30% weapon damage and skill power 10 s.
- Supply Bundle: random: 500 shield or +15% attack speed, 10 s; empowered drops a bundle giving 15 ultimate gauge.
- Manastone Bundle: +2% weapon dmg/skill power/crit, stacking, kept through tag, halved on death.
- Trickster Bundle: ally +30% ms 3 s; enemy pickup binds 2 s.
- Ultimate FIRESALE: knocks back nearby enemies, throws every bundle. Deal Chaser: +ms 2 s.
</details>

## Nedien (NAGA)

- [ ] **Passif — Creeping Venom** 
  - Douses struck enemies with venom when using a skill, and on every third weapon attack. At 3 stacks, the venom inflicts a burst of damage and restores 4% of Nedien's maximum HP.
  - serveur : géré (ICEQUEEN)
- [ ] **Sort 2 — Toxin Toss** (recharge 16s, dégâts de base 155)
  - Throws a bottle of toxic fumes at target area. The bottle lets loose fumes in an area that deal minor damage over time, stack Creeping Venom, and slows by {skill:180340020:status:stat:mul:MOVESPEED}% for {skill:180340020:status:durationtime}.
    - MOVE MOVE_PRESET_SEE_TARGET
    - `1440340023` NONETARGET SPHERE LengthX=800, sans dégâts, traverse
      - `1440340024` NONETARGET SPHERE LengthX=800, dégâts, traverse, statuts: SLOW 1.5s, POISON 3.0s, BUSH_REVEAL 3.0s **[type non géré]**
- [ ] **Sort 3 — Vitriolic Rush** (recharge 16s, dégâts de base 120)
  - Charges to a target area, striking all enemies in your path and gaining CC immunity. At the end of the charge, Nedien lets loose a toxic explosion that restricts weapon attacks and terrifies monsters for 2 seconds.
    - statut sur soi/cible : SUPERARMOR 0.85s
    - GRAPH_MOVE_HORZ 1100
    - MOVE MOVE_PRESET_TARGET_POS
    - `1440340052` NONETARGET SPHERE LengthX=600, dégâts, traverse
    - `1440340055` NONETARGET SPHERE LengthX=900, sans dégâts, traverse, statuts: SILENCE 1.0s **[type non géré]**, POISON 3.0s, BUSH_REVEAL 3.0s **[type non géré]**
- [ ] **Esquive — Skitter** (recharge 0.3s)
  - Quickly skitter 6m. While skittering, you gain CC immunity.
    - statut sur soi/cible : SUPERARMOR 0.5s
    - GRAPH_MOVE_HORZ 600
- [ ] **Sort 21 — Crippling Slingblades** (recharge 14s, dégâts de base 275)
  - Summons 3 poison-coated blades that revolve around Nedien. You may recast this skill to fling the blades like projectiles. Enemies hit by the blades are slowed by {skill:180340030:status:stat:mul:MOVESPEED}% for {skill:180340030:status:durationtime}. [Combo] If the skill is used consecutively, the e
    - `1440340040` ORBIT SPHERE LengthX=99, sans dégâts, s’arrête au 1er
    - `1440340041` ORBIT SPHERE LengthX=99, sans dégâts, s’arrête au 1er
    - `1440340042` ORBIT SPHERE LengthX=99, sans dégâts, s’arrête au 1er

<details><summary>Guide</summary>

- Bottled Miasma (toggle): poison bottle every 1 s at her position while mana lasts; -20% defense 2 s; adds venom stacks.
- Toxin Toss: bottle at an area, -30% ms 1.5 s.
- Crippling Slingblades: 3 blades orbiting her; each recast flings one; slows 10/20/30%.
- Vitriolic Rush: charge to an area, enemies can't weapon-attack 1 s.
- Ultimate Blade Frenzy: 7 s attack/move speed; venom triggers add -20% weapon damage and skill power.
- Passive Creeping Venom: venom on skill use and every 3rd attack; at 3 stacks burst + 4% max HP heal.
- Skitter (survival): 6 m dash, CC immune.
</details>

## Lua (LAUNCHER)

- [ ] **Passif — Firearms Proficiency** 
  - Upon using 5 skills, your next skill consumes no resources.
  - serveur : géré (SKILLSTATUS)
- [ ] **Sort 1 — Shrapnel Rocket** (recharge 10s, dégâts de base 815)
  - Launches a destructive rocket in a straight line. The rocket explodes on the first target it strikes, or at the end of it's range, and showers an 180 degree {skill:180350010:remote:range} radius behind the explosion point with damaging shrapnel.
    - `1440350010` NONETARGET BOX LengthX=90 LengthY=50 portée=2000, sans dégâts, s’arrête au 1er
      - `1440350011` NONETARGET SPHERE LengthX=1000, dégâts, traverse
    - GRAPH_MOVE_HORZ -200
- [ ] **Sort 2 — Distortion Shell** (recharge 12s)
  - Launches Lua herself 10m in the targeted direction with an explosive shell. The shell creates a {skill:180350020:status:targetingrange} area on the launch point that restricts weapon attacks for enemies within it.
    - statut sur soi/cible : SUPERARMOR 0.5s
    - `1440350021` NONETARGET SPHERE LengthX=600, sans dégâts, traverse
    - ENTITY 100041070
    - GRAPH_MOVE_HORZ 1000
    - statut sur soi/cible : STAT 3.0s
- [ ] **Sort 3 — Claymore Charge** (recharge 16s, dégâts de base 465)
  - Dashes to a target and pushes them away. Lua then launches a missile at the target that knocks them further back and deals damage after a short delay.
    - `1440350030` TARGET SPHERE LengthX=200 portée≈1500, dégâts, s’arrête au 1er, statuts: PUSH 0.5s
    - MOVE MOVE_PRESET_GOTO_CURRENT_TARGET
    - `1440350031` TARGET SPHERE LengthX=300 portée≈1500, dégâts, s’arrête au 1er, statuts: PUSH 1.0s
- [ ] **Sort 4 — Molten Rain** (recharge 12s, dégâts de base 840)
  - Launches a rocket into the air that splits into several micro-rockets and rains onto the targeted area, dealing heavy damage. Creates {skill:180350040:leveltext:012:4:345:7:6:8} explosions within {skill:180350040:remote:range}.
    - `1440350042` NONETARGET SPHERE LengthX=250, sans dégâts, traverse
      - `1440350043` NONETARGET SPHERE LengthX=250, dégâts, traverse
- [ ] **Ultime — Napalm Wave** (recharge 30s, dégâts de base 785)
  - Fire an expanding wave of fiery death in a cone in front of Lua. Launches enemies into the air.
    - `1440350058` NONETARGET BOX LengthX=750 LengthY=1000, sans dégâts, traverse
    - `1440350050` NONETARGET BOX LengthX=350 LengthY=200, dégâts, traverse, statuts: AIRBORNE 1.5s
      - `1440350051` NONETARGET BOX LengthX=450 LengthY=200, dégâts, traverse, statuts: AIRBORNE 1.5s
        - `1440350052` NONETARGET BOX LengthX=550 LengthY=200, dégâts, traverse, statuts: AIRBORNE 1.5s
          - `1440350053` NONETARGET BOX LengthX=650 LengthY=200, dégâts, traverse, statuts: AIRBORNE 1.5s
            - `1440350054` NONETARGET BOX LengthX=750 LengthY=200, dégâts, traverse, statuts: AIRBORNE 1.5s
    - GRAPH_MOVE_HORZ -300
- [ ] **Esquive — Swift Roll** (recharge 0.3s)
  - Quickly rolls 5m.
    - GRAPH_MOVE_HORZ 500

<details><summary>Guide</summary>

- Passive Firearms Proficiency: a free (costless) skill after using 5 skills.
- Shrapnel Rocket: straight rocket, "explodes on the first target it hits or when it reaches the end of its range", cone damage after explosion.
- Claymore Charge: dashes to a target and pushes it, then a missile knocks it further back.
- Molten Rain: missile at a target area, 4 explosions, 1 s delay before activation.
- Distortion Shell: jumps 10 m, 6 m zone at the launch point restricting enemy weapon attacks.
- Ultimate Napalm Wave: cone of successive explosions from the cast point, knocks enemies airborne.
- Swift Roll: 5 m roll, large stamina cost.
</details>

## Statesman (STATESMAN)

- [ ] **Passif — Unyielding** 
  - Crowd Control effect durations are reduced by 25%.
  - serveur : **non géré** (aucune donnée)
- [ ] **Sort 1 — Olympian Bolts** (recharge 9s, dégâts de base 250)
  - Calls down 4 lightning bolts that deal damage and slow struck enemies by {skill:180360010:status:stat:mul:movespeed}% for {skill:180360010:status:durationtime}. [Heroism] Calls down 4 additional lightning bolts.
    - MOVE MOVE_PRESET_MOVE_NONE
    - `1440360010` NONETARGET SPHERE LengthX=400, sans dégâts, traverse
      - `1440360011` NONETARGET SPHERE LengthX=400, dégâts, traverse, statuts: SLOW 4.0s
- [ ] **Sort 2 — Flying Haymaker** (recharge 14s, dégâts de base 455)
  - Quickly flies to the target location and strikes an enemy within a {skill:180360020:remote:range} radius. [Heroism] Damaged targets are launched into the air.
    - `1440360050` NONETARGET SPHERE LengthX=600, sans dégâts, traverse
    - GRAPH_MOVE_HORZ 500
    - MOVE MOVE_PRESET_TARGET_POS
    - `1440360020` NONETARGET SPHERE LengthX=600, dégâts, traverse
    - `1440360025` NONETARGET SPHERE LengthX=600, dégâts, traverse, statuts: AIRBORNE 1.5s
- [ ] **Sort 3 — Foot Stomp** (recharge 12s, dégâts de base 285)
  - Causes a shock wave within a {skill:180360030:remote:range} radius that knocks enemies back. [Heroism] Increases the knockback distance.
    - GRAPH_MOVE_HORZ 300
    - `1440360033` NONETARGET SPHERE LengthX=900, sans dégâts, traverse
    - `1440360030` NONETARGET SPHERE LengthX=900, dégâts, traverse, statuts: PUSH 0.5s
    - `1440360038` NONETARGET SPHERE LengthX=900, sans dégâts, traverse
    - `1440360035` NONETARGET SPHERE LengthX=900, dégâts, traverse, statuts: PUSH 1.0s
- [ ] **Sort 4 — Dull Pain** (recharge 10s)
  - Heals {skill:180360040:status:regen:mul:health}% max HP and removes all debuffs. Restores an additional 2% max HP for each debuff removed. [Heroism] Spreads the same effect to allied Masters within a {skill:180360040:remote:range:4} radius.
    - MOVE MOVE_PRESET_MOVE_NONE
    - statut sur soi/cible : HEAL 1.0s
    - statut sur soi/cible : DISPEL 0.1s
    - `1440360040` NONETARGET SPHERE LengthX=1200, sans dégâts, traverse, statuts: HEAL 1.0s, DISPEL 0.1s
- [ ] **Esquive — Unstoppable** (recharge 0.3s)
  - Become immune to all damage and crowd control effects for {skill:180360002:status:durationtime}. Movespeed increased by 15% for 2 seconds after using Unstoppable.
    - statut sur soi/cible : GODMODE 1.5s
    - statut sur soi/cible : STAT 2.0s

<details><summary>Guide</summary>

- Heroism: flies 8 s, +5% ms, +10% defense, ranged weapon attacks, empowered skills; resets non-ult cooldowns, +20% MP.
- Ultimate Olympian Bolts: 4 lightning bolts, base 50 dmg, slow 30% 4 s, cd 9 s.
- Flying Haymaker: flies to the target location and strikes; airborne during Heroism; cd 14 s.
- Foot Stomp: shockwave knockback, 285 dmg; Haymaker + Stomp binds.
- Dull Pain: heal 10% max HP, removes all debuffs, +2% per debuff; under Heroism also allies within 6 m.
- Unstoppable (survival): immune to damage and CC 2 s, then +15% ms 2 s. Passive: CC duration -25%.
</details>

## Yuri (name unverified) (YURI)

- [ ] **Passif — Martial Master** 
  - [Triage] Summons a medical aid when your health drops below 30%. The aid stuns nearby enemies for {skill:180370000:status:durationtime} and restores 5% max HP per second every 5 seconds to nearby allied Masters. Has a 2 minute cool down. [Catalyst of Sol] When your Sol power is full, skills gain add
  - serveur : **non géré** (HPTRIGGER)
- [ ] **Sort 1 — Sol Fist** (recharge 12s, dégâts de base 695)
  - Lunge forward 1.5m and unleash a powerful punch. [Catalyst of Sol] Slows by {skill:180370010:status:stat:mul:movespeed}% for {skill:180370010:status:durationtime}.
    - `1440370010` NONETARGET BOX LengthX=200 LengthY=450, dégâts, traverse, statuts: FLAG 0.5s
    - GRAPH_MOVE_HORZ 150
- [ ] **Sort 2 — Flanking Strike** (recharge 15s, dégâts de base 270)
  - Dash behind your target and strike their back. [Catalyst of Sol] Stun your target for {skill:180370020:status:durationtime}.
    - MOVE MOVE_PRESET_GOTO_CURRENT_TARGET_THROUGH
    - `1440370020` TARGET BOX LengthX=150 LengthY=200 portée≈300, dégâts, s’arrête au 1er
    - statut sur soi/cible : FLAG 0.1s
- [ ] **Sort 3 — Sol Kick** (recharge 15s, dégâts de base 820)
  - Dash 10m in a flying kick, damaging all enemies in your path. Targets launched into the air take 25% increased damage. [Catalyst of Sol] Sol Kick shreds defenses by {skill:180370030:status:stat:mul:defence}% for {skill:180370030:status:durationtime}.
    - `1440370030` NONETARGET BOX LengthX=180 LengthY=300, dégâts, traverse
    - GRAPH_MOVE_HORZ 1000
- [ ] **Ultime — Sol Crush** (recharge 15s, dégâts de base 330)
  - Dash forward 8m and strike enemies with a barrage of crushing punches, stunning them briefly. [Catalyst of Sol] The final blow launches enemies in the air.
    - statut sur soi/cible : FLAG 1.0s
    - statut sur soi/cible : SUPERARMOR 2.0s
    - GRAPH_MOVE_HORZ 800
    - `1440370050` NONETARGET BOX LengthX=200 LengthY=220, dégâts, s’arrête au 1er, statuts: STUN 0.5s
- [ ] **Esquive — Sol Boosting** 
  - Sol Boost Ends
- [ ] **Sort 20 — Cosmic Flurry** (recharge 12s, dégâts de base 315)
  - Your next attack within {skill:180370040:status:durationtime} deals 3 successive blows. [Catalyst of Sol] Reflect projectiles in front of you while attacking.
    - `1440370040` NONETARGET BOX LengthX=220 LengthY=250, dégâts, traverse
    - statut sur soi/cible : FLAG 0.1s

## Xero (ESPER)

- [ ] **Passif — Holo-Cube Engraving** 
  - [Pattern Transfer] Weapon attacks mark targets. After two strikes hit the same target, they will be marked with Xero's Engraving. Engraved enemies become the target of your skills. Skills cannot be used if no targets are engraved. Additional attacks do not refresh the engraving duration. A maximum o
  - serveur : **non géré** (STACKATTACK)
- [ ] **Sort 1 — Engraved Burst** (recharge 5s, dégâts de base 800)
  - The area around each engraved target explodes and deals damage to the target and its surroundings in a {skill:180380010:remote:range} range. Cannot be used if no engraved targets are in range.
- [ ] **Sort 2 — Engraved Erosion** (recharge 7s, dégâts de base 100)
  - Your engraved patterns distort the minds of your targets. For {skill:180380020:status:durationtime} Weapon Damage is reduced by {skill:180380020:status:stat:mul:attack}%, Skill Power by {skill:180380020:status:stat:add:skilldamage_ratio}%, and Defense by {skill:180380020:status:stat:mul:defence}%. U
- [ ] **Sort 3 — Engraved Telekinesis** (recharge 8s, dégâts de base 50)
  - Jump back {skill:180380030:actionbase:movehorizon} and pull in all engraved targets to your location. Slows pulled targets by {skill:180380030:status:stat:mul:movespeed:5}% for {skill:180380030:status:durationtime:5}. Cannot be used if no engraved targets are in range.
    - GRAPH_MOVE_HORZ -400
- [ ] **Sort 4 — Engraved Shockwave** (recharge 6s, dégâts de base 50)
  - Stuns engraved targets for {skill:180380040:status:durationtime}. Cannot be used if no engraved targets are in range.
- [ ] **Ultime — Holo-Cube Pulse** (recharge 15s, dégâts de base 720)
  - Deals damage to all enemies within {skill:180380050:remote:range} and engraves them immediately.
    - `1440380021` NONETARGET SPHERE LengthX=1400, dégâts, traverse
- [ ] **Esquive — Holo-Hop** (recharge 0.3s)
  - Hop {skill:180380002:actionbase:movehorizon} while evading enemy attacks.
    - statut sur soi/cible : EVADE 0.8s
    - GRAPH_MOVE_HORZ 750

## Titus (PRIEST)

- [ ] **Passif — The Light Within** 
  - Titus emanates holy energy as he charges his weapon attack. Heals 30hp per second within 10m. Range increased to 12m at stage 2 Range increased to 15m at stage 3.
  - serveur : **non géré** (aucune donnée)
- [ ] **Sort 1 — Mending Palm** (recharge 8s)
  - Instantly restores {skill:180390010:status:regen:mul:health}% max health to target ally Masters or yourself.
    - statut sur soi/cible : HEAL 0.5s
- [ ] **Sort 2 — Purifying Edict** (recharge 13s, dégâts de base 240)
  - Fires an array of sacred bolts that chain through allies and enemies alike, restoring {skill:180390020:status:regen:add:health} HP to allies and dealing damage to enemies. The bolts prioritize Masters, including the caster. Can chain up to 5 times.
    - `1440390020` CHAIN BEAM LengthX=40, dégâts, traverse, alliés: HEAL 0.1s
      - `1440390021` CHAIN BEAM LengthX=40, dégâts, traverse, alliés: HEAL 0.1s
        - `1440390022` CHAIN BEAM LengthX=40, dégâts, traverse, alliés: HEAL 0.1s
          - `1440390023` CHAIN BEAM LengthX=40, dégâts, traverse, alliés: HEAL 0.1s
            - `1440390024` CHAIN BEAM LengthX=40, dégâts, traverse, alliés: HEAL 0.1s
- [ ] **Sort 3 — Nura's Grace** (recharge 14s)
  - Creates a sacred road in a line in front of you. Allies on the road regain {skill:180390030:status:regen:add:health} HP each second. Enemies that touch the road are slowed by {skill:180390030:status:stat:mul:movespeed:1}%.
    - `1440390030` NONETARGET BOX LengthX=300 LengthY=500, sans dégâts, traverse, statuts: SLOW 1.0s, alliés: HEAL 1.0s, FLAG 1.0s
      - `1440390031` NONETARGET BOX LengthX=300 LengthY=500, sans dégâts, traverse, statuts: SLOW 1.0s, alliés: HEAL 1.0s, FLAG 1.0s
        - `1440390032` NONETARGET BOX LengthX=300 LengthY=500, sans dégâts, traverse, statuts: SLOW 1.0s, alliés: HEAL 1.0s, FLAG 1.0s
    - `1440390035` NONETARGET BOX LengthX=300 LengthY=1500, sans dégâts
- [ ] **Sort 4 — Hallowed Ground** (recharge 16s)
  - Creates an area of Hallowed Ground with a {skill:180390040:status:targetingrange} radius at the selected spot. Allies within the area have their debuffs cleansed and receive CC Immunity while within the area. Enemies standing on Hallowed Ground are silenced. Non-Master targets will be terrorized ins
- [ ] **Ultime — Divine Intervention** (recharge 90s)
  - Casts a prayer of resurrection on allies within a {skill:180390050:status:targetingrange} radius. If marked allies are dealt lethal damage, Nura intervenes and immediately restores {skill:180390050:status:paramratio}% of their max HP. When Divine Intervention is triggered, the target becomes invinci
    - statut sur soi/cible : SECOND_CHANCE 5.0s **[type non géré]**
    - statut sur soi/cible : STAT 1.0s
- [ ] **Esquive — Divine Directive** (recharge 0.3s)
  - After a 0.5s delay, Titus will blink forward 10m in the direction you're facing or moving.
    - MOVE MOVE_PRESET_WARP

## Ghost Widow (GHOSTWIDOW)

- [ ] **Passif — Consuming Darkness** 
  - [Consumption] Increases Ghost Widow's Weapon Damage by 12% per debuff on her target.
  - serveur : géré (GHOSTWIDOW)
- [ ] **Sort 1 — Life Drain** (recharge 13s)
  - Tap into the powers of the netherworld to steal life from a target foe for {skill:180400010:status:durationtime}, damaging them every 1.5 seconds and healing Ghost Widow. If your target moves too far away, the draining tether will snap.
    - statut sur soi/cible : LINK 7.55s **[type non géré]**
    - statut sur soi/cible : POISON 7.5s
    - statut sur soi/cible : FLAG 0.01s
- [ ] **Sort 2 — Fearsome Stare** (recharge 24s, dégâts de base 230)
  - Fires a projectile of negative energy that strikes the first target in a line. The projectile bursts on its target, striking all hostiles within {skill:180400020:remote:range} around the targeted enemy and terrorizing them for {skill:180400020:status:durationtime}.
    - `1440400020` NONETARGET BOX LengthX=100 LengthY=200 portée=1600, sans dégâts, s’arrête au 1er
      - `1440400021` NONETARGET SPHERE LengthX=600, dégâts, traverse, statuts: FEAR 1.8s
- [ ] **Sort 3 — Tenebrous Tentacles** (recharge 15s)
  - Curses an enemy with darkness, damaging them for {skill:180400030:status:durationtime}. Other enemies in a radius of {skill:180400030:status:targetingrange} around the target are dealt 50% of the damage over time.
    - statut sur soi/cible : DEFEAT_CASTER_AURA 5.005s **[type non géré]**
    - statut sur soi/cible : POISON 5.005s
- [ ] **Ultime — Soul Storm** (recharge 30s, dégâts de base 360)
  - Enraptures a target for {skill:180400050:status:durationtime} and drains the life from them. Unleashes 3 blasts of necrotic energy and generates two missiles of dark energy each time Ghost Widow deals damage. Moving or attacking will cancel Soul Storm.
- [ ] **Esquive — Dark Passage** (recharge 0.3s)
  - Dashes {skill:180400020:remote:range} and strips non-persistent buffs from all targets she collides with.
    - `1440400010` NONETARGET BOX LengthX=250 LengthY=250, sans dégâts, traverse, statuts: DISPEL 0.1s
    - GRAPH_MOVE_HORZ 725

<details><summary>Guide</summary>

- Passive Consuming Darkness: +12% weapon damage per debuff on the target.
- Life Drain: tether 7.5 s, damage every 1.5 s, heals her, breaks if too far; she can move and attack.
- Fearsome Stare: projectile hits the FIRST target in line (no pierce), fear 1.8 s, explodes on impact or max range.
- Tenebrous Tentacles: DoT 5 s on the target, others in a small circle take 50% of it.
- Ethereal Images: ghost form 3 s, 3 copies roam then explode, immune 3 s, can't attack/cast/jump.
- Ultimate Soul Storm: target lifted 3.5 s, 3 blasts, each hit spawns 2 missiles; channel (moving/attacking cancels).
- Dark Passage: 3 m dash stripping buffs from enemies hit.
</details>

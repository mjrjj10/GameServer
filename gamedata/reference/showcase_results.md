# Résultats du banc de test des masters (40/40)

Mode DevShowcase : chaque sort est lancé par un bot sur un mannequin à plat, près, juste dans la portée (portée - 40) et juste au-delà
(portée réelle tirée des données : trajet du projectile + avant de sa boîte + rayon des effets + rayon de collision du mannequin + 60).
Les visuels de chaque test sont comparés aux 200 previews officielles des sorts (récupérées via le JS archivé de playmxm.com).

Évolution : 119 écarts → 89 (rayon de collision réel) → 75 (previews) → 38 (passe de corrections complète) → 32 (r30) → 29 (r31 : délais absolus, pas de collision entre personnages, charges progressives) ; postures ajoutées ensuite (q16).

## Limites connues
- Charges des bots : la courbe de vitesse (GRAPH_MOVE_HORZ Param2) est dans les animations, illisibles ; la charge dure toute l'action,
  donc les boîtes attachées de courte durée (Yuri 3, Ignuma 3) n'atteignent pas la fin. Un joueur humain charge via son client.
- Coup de suite au contact (_ChangeCasterActionState) pour un joueur humain : non vérifié côté client.
- Écarts « range_in » à 40 cm du bord des sorts en ligne : la longueur d'indicateur (_SkillRangeLengthY) dépasse légèrement la portée
  réelle des effets ; non ajusté faute de donnée.
- Table ClassAttackRange (portée d'arme par master) sans source ; utilisée seulement quand l'arme n'a pas de projectile.
- Formule de défense non implémentée.

## Taejin : 16 tests, 0 écarts
## MBA-07 : 11 tests, 0 écarts
## Sizuka : 13 tests, 1 écarts
- SKILL_3 180030030 range_out (portée 800, distance 983) : touché=1 coups=4 dégâts=260 ÉCART
## Demenos : 20 tests, 3 écarts
- WEAPON 0 range_out (portée 800, distance 953) : touché=1 coups=1 dégâts=120 ÉCART
- SKILL_3 180040030 range_out (portée 1000, distance 1133) : touché=1 coups=1 dégâts=212 ÉCART
- SKILL_4 180040040 range_out (portée 720, distance 853) : touché=1 coups=1 dégâts=386 ÉCART
## Koom : 12 tests, 1 écarts
- SKILL_4 180050040 range_in (portée 1100, distance 1060) : touché=0 coups=0 dégâts=0 ÉCART
## Innowin : 13 tests, 2 écarts
- SKILL_1 180060040 range_in (portée 1800, distance 1760) : touché=0 coups=0 dégâts=0 ÉCART
- SKILL_4 180060030 range_in (portée 1360, distance 1320) : touché=0 coups=0 dégâts=0 ÉCART
## Death Knight : 10 tests, 1 écarts
- SKILL_1 180070010 range_in (portée 900, distance 860) : touché=0 coups=0 dégâts=0 ÉCART
## Cagnazzo : 13 tests, 1 écarts
- SKILL_2 180080020 range_out (portée 900, distance 1405) : touché=1 coups=1 dégâts=80 ÉCART
## Moro : 5 tests, 0 écarts
## Arami : 5 tests, 1 écarts
- WEAPON 0 splash_in (portée 1100, distance 660) : touché=0 coups=0 dégâts=0 ÉCART
## Vonak : 4 tests, 0 écarts
## Jinsoyun : 14 tests, 0 écarts
## Kromede : 13 tests, 2 écarts
- SKILL_2 180130020 range_in (portée 1770, distance 1730) : touché=0 coups=0 dégâts=0 ÉCART
- SKILL_4 180130040 range_out (portée 600, distance 705) : touché=1 coups=0 dégâts=0 ÉCART
## Jingtai : 11 tests, 3 écarts
- SKILL_1 180140010 range_in (portée 660, distance 620) : touché=0 coups=0 dégâts=0 ÉCART
- SKILL_2 180140020 range_out (portée 860, distance 1140) : touché=1 coups=1 dégâts=287 ÉCART
- ULTIMATE 180140050 range_in (portée 1650, distance 1610) : touché=0 coups=0 dégâts=0 ÉCART
## Lorraine : 11 tests, 0 écarts
## Eirna : 7 tests, 0 écarts
## Vita : 14 tests, 0 écarts
## Poharan : 15 tests, 3 écarts
- SKILL_2 180180020 near (portée 1000, distance 300) : touché=0 coups=0 dégâts=0 ÉCART
- SKILL_2 180180020 range_in (portée 1000, distance 960) : touché=0 coups=0 dégâts=0 ÉCART
- SKILL_4 180180040 range_out (portée 600, distance 835) : touché=1 coups=1 dégâts=287 ÉCART
## V-Merang : 18 tests, 0 écarts
## Bumaro : 8 tests, 0 écarts
## Mondo Zax : 3 tests, 0 écarts
## Lilu : 15 tests, 0 écarts
## Ignuma : 11 tests, 2 écarts
- SKILL_3 180230030 near (portée 900, distance 270) : touché=0 coups=0 dégâts=0 ÉCART
- SKILL_3 180230030 range_in (portée 900, distance 860) : touché=0 coups=0 dégâts=0 ÉCART
## Dr. Raoul : 11 tests, 1 écarts
- SKILL_4 180240040 range_out (portée 1300, distance 1440) : touché=1 coups=1 dégâts=250 ÉCART
## Rytlock : 10 tests, 1 écarts
- SKILL_1 180250010 range_in (portée 1000, distance 960) : touché=0 coups=0 dégâts=0 ÉCART
## Kat : 5 tests, 0 écarts
## R&B : 15 tests, 1 écarts
- WEAPON 0 splash_in (portée 0, distance 300) : touché=0 coups=0 dégâts=0 ÉCART
## Sonid : 10 tests, 1 écarts
- SKILL_4 180280040 range_in (portée 1400, distance 1360) : touché=0 coups=0 dégâts=0 ÉCART
## Merope : 13 tests, 1 écarts
- SKILL_3 180290030 range_in (portée 300, distance 260) : touché=0 coups=0 dégâts=0 ÉCART
## Maia : 12 tests, 0 écarts
## Tulam : 11 tests, 2 écarts
- SKILL_2 180310020 range_in (portée 800, distance 760) : touché=0 coups=0 dégâts=0 ÉCART
- SKILL_3 180310030 range_in (portée 1200, distance 1160) : touché=0 coups=0 dégâts=0 ÉCART
## Pancuga : 2 tests, 0 écarts
## Nanurunerk : 18 tests, 1 écarts
- WEAPON 0 range_out (portée 1250, distance 1355) : touché=1 coups=1 dégâts=120 ÉCART
## Nedien : 8 tests, 1 écarts
- SKILL_3 180340040 range_in (portée 1100, distance 1060) : touché=0 coups=0 dégâts=0 ÉCART
## Lua : 18 tests, 2 écarts
- SKILL_4 180350040 range_out (portée 1200, distance 1465) : touché=1 coups=1 dégâts=714 ÉCART
- ULTIMATE 180350050 range_out (portée 500, distance 1015) : touché=1 coups=1 dégâts=667 ÉCART
## Statesman : 8 tests, 1 écarts
- SKILL_2 180360020 range_in (portée 1000, distance 960) : touché=0 coups=0 dégâts=0 ÉCART
## Yuri : 12 tests, 2 écarts
- SKILL_3 180370030 range_in (portée 1000, distance 960) : touché=0 coups=0 dégâts=0 ÉCART
- ULTIMATE 180370050 range_in (portée 500, distance 460) : touché=0 coups=0 dégâts=0 ÉCART
## Xero : 8 tests, 0 écarts
## Titus : 17 tests, 1 écarts
- SKILL_1 180390010 range_out (portée 1400, distance 1540) : touché=1 coups=1 dégâts=212 ÉCART
## Ghost Widow : 17 tests, 3 écarts
- SKILL_1 180400010 range_out (portée 1000, distance 1140) : touché=1 coups=1 dégâts=250 ÉCART
- SKILL_3 180400030 range_out (portée 1100, distance 1240) : touché=1 coups=1 dégâts=250 ÉCART
- ULTIMATE 180400050 range_out (portée 600, distance 740) : touché=1 coups=1 dégâts=380 ÉCART

# Résultats du banc de test des masters (40/40)

Généré automatiquement par le mode DevShowcase : chaque sort est lancé par un bot sur un mannequin placé à plat, à courte portée, juste dans la portée annoncée et juste au-delà (portée + rayon de la zone de dégâts tiré des données).

`ÉCART` = résultat différent de l’attendu. Un `range_out` touché signifie que la zone réelle dépasse la portée annoncée : soit c’est normal (projectile qui continue, zone plus large), soit c’est un bug à confirmer visuellement.

## Écarts connus déjà diagnostiqués

- Charges et ruées des bots (Maia 1, Lilu 3, Jingtai 2) : les mouvements GOTO_CURRENT_TARGET / LONG_JUMP et le coup de suite `_ChangeCasterActionState` ne sont pas exécutés pour les bots (le client le fait pour un joueur humain).
- Koom 4 : sort défensif, l’attente de dégâts du banc est fausse.
- Innowin et Xero : armes chargées, le banc n’émule pas la charge.
- Nanurunerk : le tir en cloche qui atterrit pile sur la cible n’est pas compté.
- Formule de défense non implémentée (dégâts plus élevés qu’en vidéo).

## Taejin : 16 tests, 3 écarts
- WEAPON 0 range_out (portée 1060, distance 1120) : touché=1 coups=1 dégâts=120 ÉCART
- SKILL_3 180010010 range_out (portée 500, distance 710) : touché=1 coups=1 dégâts=550 ÉCART
- ULTIMATE 180010050 range_out (portée 1100, distance 1560) : touché=1 coups=5 dégâts=3400 ÉCART
## MBA-07 : 11 tests, 3 écarts
- WEAPON 0 range_out (portée 1600, distance 1660) : touché=1 coups=1 dégâts=120 ÉCART
- WEAPON 0 splash_out (portée 1600, distance 960) : touché=1 coups=1 dégâts=120 ÉCART
- SKILL_1 180020010 range_out (portée 1500, distance 1760) : touché=1 coups=1 dégâts=459 ÉCART
## Sizuka : 13 tests, 3 écarts
- SKILL_2 180030020 range_out (portée 1000, distance 1110) : touché=1 coups=1 dégâts=212 ÉCART
- SKILL_3 180030030 range_out (portée 800, distance 910) : touché=1 coups=4 dégâts=260 ÉCART
- ULTIMATE 180030050 range_out (portée 500, distance 660) : touché=1 coups=1 dégâts=510 ÉCART
## Demenos : 20 tests, 6 écarts
- WEAPON 0 range_out (portée 800, distance 860) : touché=1 coups=2 dégâts=222 ÉCART
- WEAPON 0 splash_out (portée 800, distance 480) : touché=1 coups=1 dégâts=102 ÉCART
- SKILL_1 180040010 range_out (portée 900, distance 1360) : touché=1 coups=1 dégâts=59 ÉCART
- SKILL_3 180040030 range_out (portée 1000, distance 1060) : touché=1 coups=1 dégâts=212 ÉCART
- SKILL_4 180040040 range_out (portée 720, distance 780) : touché=1 coups=1 dégâts=386 ÉCART
- ULTIMATE 180040050 range_out (portée 1000, distance 1470) : touché=1 coups=1 dégâts=799 ÉCART
## Koom : 12 tests, 4 écarts
- WEAPON 0 range_out (portée 450, distance 510) : touché=1 coups=1 dégâts=120 ÉCART
- SKILL_2 180050020 range_out (portée 600, distance 935) : touché=1 coups=0 dégâts=0 ÉCART
- SKILL_4 180050040 near (portée 1100, distance 330) : touché=0 coups=0 dégâts=0 ÉCART
- SKILL_4 180050040 range_in (portée 1100, distance 1060) : touché=0 coups=0 dégâts=0 ÉCART
## Innowin : 13 tests, 4 écarts
- WEAPON 0 range_in (portée 1000, distance 960) : touché=0 coups=0 dégâts=0 ÉCART
- SKILL_1 180060040 near (portée 200, distance 150) : touché=0 coups=0 dégâts=0 ÉCART
- SKILL_1 180060040 range_in (portée 200, distance 160) : touché=0 coups=0 dégâts=0 ÉCART
- SKILL_4 180060030 range_out (portée 100, distance 205) : touché=1 coups=1 dégâts=132 ÉCART
## Death Knight : 10 tests, 2 écarts
- SKILL_1 180070010 range_out (portée 300, distance 510) : touché=1 coups=1 dégâts=310 ÉCART
- SKILL_2 180070020 range_out (portée 700, distance 1160) : touché=1 coups=2 dégâts=560 ÉCART
## Cagnazzo : 13 tests, 4 écarts
- WEAPON 0 range_out (portée 600, distance 660) : touché=1 coups=1 dégâts=138 ÉCART
- SKILL_1 180080010 range_out (portée 200, distance 305) : touché=1 coups=1 dégâts=793 ÉCART
- SKILL_2 180080020 range_out (portée 900, distance 1360) : touché=1 coups=1 dégâts=80 ÉCART
- SKILL_4 180080040 range_out (portée 500, distance 810) : touché=1 coups=1 dégâts=362 ÉCART
## Moro : 5 tests, 2 écarts
- WEAPON 0 range_out (portée 400, distance 460) : touché=1 coups=1 dégâts=120 ÉCART
- WEAPON 0 splash_out (portée 400, distance 240) : touché=1 coups=1 dégâts=120 ÉCART
## Arami : 5 tests, 1 écarts
- WEAPON 0 range_out (portée 1100, distance 1160) : touché=1 coups=2 dégâts=222 ÉCART
## Vonak : 4 tests, 1 écarts
- WEAPON 0 range_out (portée 900, distance 960) : touché=1 coups=1 dégâts=138 ÉCART
## Jinsoyun : 14 tests, 3 écarts
- SKILL_3 180120030 range_out (portée 1000, distance 1300) : touché=1 coups=1 dégâts=582 ÉCART
- SKILL_4 180120040 range_out (portée 1000, distance 1260) : touché=1 coups=1 dégâts=459 ÉCART
- ULTIMATE 180120050 range_out (portée 800, distance 873) : touché=1 coups=1 dégâts=769 ÉCART
## Kromede : 13 tests, 4 écarts
- WEAPON 0 range_out (portée 1200, distance 1260) : touché=1 coups=1 dégâts=102 ÉCART
- SKILL_2 180130020 range_out (portée 200, distance 275) : touché=1 coups=1 dégâts=357 ÉCART
- SKILL_3 180130030 range_out (portée 1000, distance 1460) : touché=1 coups=5 dégâts=850 ÉCART
- SKILL_4 180130040 range_out (portée 600, distance 660) : touché=1 coups=0 dégâts=0 ÉCART
## Jingtai : 11 tests, 4 écarts
- SKILL_1 180140010 range_out (portée 300, distance 500) : touché=1 coups=3 dégâts=1449 ÉCART
- SKILL_2 180140020 near (portée 400, distance 150) : touché=0 coups=0 dégâts=0 ÉCART
- SKILL_2 180140020 range_in (portée 400, distance 360) : touché=0 coups=0 dégâts=0 ÉCART
- ULTIMATE 180140050 range_out (portée 500, distance 680) : touché=1 coups=1 dégâts=989 ÉCART
## Lorraine : 11 tests, 2 écarts
- SKILL_2 180150020 range_out (portée 1500, distance 1560) : touché=1 coups=0 dégâts=0 ÉCART
- ULTIMATE 180150050 range_out (portée 1000, distance 1410) : touché=1 coups=1 dégâts=769 ÉCART
## Eirna : 7 tests, 2 écarts
- WEAPON 0 range_in (portée 1000, distance 960) : touché=0 coups=0 dégâts=0 ÉCART
- SKILL_3 180160030 range_in (portée 1100, distance 1060) : touché=0 coups=0 dégâts=0 ÉCART
## Vita : 14 tests, 2 écarts
- WEAPON 0 range_in (portée 1150, distance 1110) : touché=0 coups=0 dégâts=0 ÉCART
- SKILL_3 180170030 range_out (portée 200, distance 365) : touché=1 coups=1 dégâts=315 ÉCART
## Poharan : 15 tests, 3 écarts
- WEAPON 0 range_in (portée 1200, distance 1160) : touché=0 coups=0 dégâts=0 ÉCART
- SKILL_1 180180010 range_out (portée 850, distance 1090) : touché=1 coups=2 dégâts=856 ÉCART
- SKILL_4 180180040 range_out (portée 250, distance 310) : touché=1 coups=1 dégâts=287 ÉCART
## V-Merang : 18 tests, 5 écarts
- WEAPON 0 range_in (portée 1000, distance 960) : touché=0 coups=0 dégâts=0 ÉCART
- SKILL_1 180190010 range_out (portée 1200, distance 1435) : touché=1 coups=1 dégâts=540 ÉCART
- SKILL_2 180190020 range_out (portée 1200, distance 1325) : touché=1 coups=1 dégâts=350 ÉCART
- SKILL_4 180190040 range_out (portée 1200, distance 1460) : touché=1 coups=4 dégâts=368 ÉCART
- ULTIMATE 180190050 range_out (portée 800, distance 1110) : touché=1 coups=1 dégâts=839 ÉCART
## Bumaro : 8 tests, 2 écarts
- WEAPON 0 range_out (portée 1200, distance 1260) : touché=1 coups=1 dégâts=120 ÉCART
- WEAPON 0 splash_out (portée 1200, distance 720) : touché=1 coups=1 dégâts=120 ÉCART
## Mondo Zax : 3 tests, 1 écarts
- WEAPON 0 range_out (portée 1200, distance 1260) : touché=1 coups=1 dégâts=120 ÉCART
## Lilu : 15 tests, 5 écarts
- WEAPON 0 range_out (portée 1100, distance 1160) : touché=1 coups=1 dégâts=138 ÉCART
- SKILL_3 180220030 near (portée 350, distance 150) : touché=0 coups=0 dégâts=0 ÉCART
- SKILL_3 180220030 range_in (portée 350, distance 310) : touché=0 coups=0 dégâts=0 ÉCART
- SKILL_4 180220040 range_out (portée 200, distance 275) : touché=1 coups=1 dégâts=115 ÉCART
- ULTIMATE 180220050 range_in (portée 2000, distance 1960) : touché=0 coups=0 dégâts=0 ÉCART
## Ignuma : 11 tests, 3 écarts
- WEAPON 0 range_out (portée 625, distance 685) : touché=1 coups=1 dégâts=102 ÉCART
- SKILL_3 180230030 near (portée 350, distance 150) : touché=0 coups=0 dégâts=0 ÉCART
- SKILL_3 180230030 range_in (portée 350, distance 310) : touché=0 coups=0 dégâts=0 ÉCART
## Dr. Raoul : 11 tests, 2 écarts
- SKILL_4 180240040 range_out (portée 1300, distance 1360) : touché=1 coups=1 dégâts=250 ÉCART
- ULTIMATE 180240050 range_out (portée 1200, distance 1860) : touché=1 coups=1 dégâts=1055 ÉCART
## Rytlock : 10 tests, 2 écarts
- SKILL_1 180250010 near (portée 1000, distance 300) : touché=0 coups=0 dégâts=0 ÉCART
- SKILL_1 180250010 range_in (portée 1000, distance 960) : touché=0 coups=0 dégâts=0 ÉCART
## Kat : 5 tests, 0 écarts
## R&B : 15 tests, 4 écarts
- WEAPON 0 splash_in (portée 0, distance 300) : touché=0 coups=0 dégâts=0 ÉCART
- SKILL_1 180270010 range_out (portée 1000, distance 1460) : touché=1 coups=1 dégâts=221 ÉCART
- SKILL_2 180270020 range_out (portée 1000, distance 1210) : touché=1 coups=1 dégâts=641 ÉCART
- SKILL_4 180270040 range_out (portée 100, distance 205) : touché=1 coups=1 dégâts=527 ÉCART
## Sonid : 10 tests, 5 écarts
- WEAPON 0 range_out (portée 1200, distance 1260) : touché=1 coups=1 dégâts=138 ÉCART
- SKILL_3 180280030 near (portée 500, distance 150) : touché=0 coups=0 dégâts=0 ÉCART
- SKILL_3 180280030 range_in (portée 500, distance 460) : touché=0 coups=0 dégâts=0 ÉCART
- SKILL_4 180280040 near (portée 150, distance 150) : touché=0 coups=0 dégâts=0 ÉCART
- SKILL_4 180280040 range_in (portée 150, distance 110) : touché=0 coups=0 dégâts=0 ÉCART
## Merope : 13 tests, 3 écarts
- WEAPON 0 range_in (portée 1350, distance 1310) : touché=0 coups=0 dégâts=0 ÉCART
- SKILL_2 180290020 range_out (portée 1200, distance 1660) : touché=1 coups=1 dégâts=445 ÉCART
- ULTIMATE 180290050 range_in (portée 500, distance 460) : touché=0 coups=0 dégâts=0 ÉCART
## Maia : 12 tests, 4 écarts
- SKILL_1 180300010 near (portée 1200, distance 360) : touché=0 coups=0 dégâts=0 ÉCART
- SKILL_1 180300010 range_in (portée 1200, distance 1160) : touché=0 coups=0 dégâts=0 ÉCART
- SKILL_2 180300020 near (portée 1000, distance 300) : touché=0 coups=0 dégâts=0 ÉCART
- SKILL_2 180300020 range_in (portée 1000, distance 960) : touché=0 coups=0 dégâts=0 ÉCART
## Tulam : 11 tests, 4 écarts
- WEAPON 0 range_out (portée 420, distance 480) : touché=1 coups=1 dégâts=120 ÉCART
- SKILL_2 180310020 range_out (portée 800, distance 1060) : touché=1 coups=1 dégâts=74 ÉCART
- SKILL_3 180310030 near (portée 1200, distance 360) : touché=0 coups=0 dégâts=0 ÉCART
- SKILL_3 180310030 range_in (portée 1200, distance 1160) : touché=0 coups=0 dégâts=0 ÉCART
## Pancuga : 2 tests, 0 écarts
## Nanurunerk : 18 tests, 4 écarts
- WEAPON 0 range_in (portée 1250, distance 1210) : touché=0 coups=0 dégâts=0 ÉCART
- SKILL_3 180330030 range_out (portée 1500, distance 1635) : touché=1 coups=0 dégâts=0 ÉCART
- SKILL_4 180330040 range_out (portée 1500, distance 1635) : touché=1 coups=0 dégâts=0 ÉCART
- ULTIMATE 180330050 range_out (portée 1200, distance 1660) : touché=1 coups=1 dégâts=799 ÉCART
## Nedien : 8 tests, 2 écarts
- SKILL_2 180340020 range_out (portée 1200, distance 1660) : touché=1 coups=3 dégâts=465 ÉCART
- SKILL_3 180340040 range_out (portée 1100, distance 1610) : touché=1 coups=0 dégâts=0 ÉCART
## Lua : 18 tests, 5 écarts
- WEAPON 0 range_out (portée 920, distance 980) : touché=1 coups=1 dégâts=105 ÉCART
- SKILL_1 180350010 range_out (portée 180, distance 740) : touché=1 coups=1 dégâts=692 ÉCART
- SKILL_2 180350020 range_out (portée 400, distance 460) : touché=1 coups=0 dégâts=0 ÉCART
- SKILL_4 180350040 range_out (portée 1200, distance 1385) : touché=1 coups=1 dégâts=714 ÉCART
- ULTIMATE 180350050 range_out (portée 500, distance 935) : touché=1 coups=1 dégâts=667 ÉCART
## Statesman : 8 tests, 0 écarts
## Yuri : 12 tests, 3 écarts
- SKILL_3 180370030 range_in (portée 250, distance 210) : touché=0 coups=0 dégâts=0 ÉCART
- SKILL_3 180370030 range_out (portée 250, distance 400) : touché=1 coups=1 dégâts=943 ÉCART
- ULTIMATE 180370050 range_in (portée 500, distance 460) : touché=0 coups=0 dégâts=0 ÉCART
## Xero : 8 tests, 1 écarts
- WEAPON 0 range_in (portée 1100, distance 1060) : touché=0 coups=0 dégâts=0 ÉCART
## Titus : 17 tests, 5 écarts
- SKILL_1 180390010 range_out (portée 1400, distance 1460) : touché=1 coups=1 dégâts=212 ÉCART
- SKILL_2 180390020 range_out (portée 1400, distance 1480) : touché=1 coups=2 dégâts=408 ÉCART
- SKILL_3 180390030 range_out (portée 310, distance 520) : touché=1 coups=0 dégâts=0 ÉCART
- SKILL_4 180390040 near (portée 1000, distance 300) : touché=0 coups=0 dégâts=0 ÉCART
- SKILL_4 180390040 range_in (portée 1000, distance 960) : touché=0 coups=0 dégâts=0 ÉCART
## Ghost Widow : 17 tests, 5 écarts
- WEAPON 0 range_out (portée 1000, distance 1060) : touché=1 coups=2 dégâts=240 ÉCART
- SKILL_1 180400010 range_out (portée 1000, distance 1060) : touché=1 coups=1 dégâts=250 ÉCART
- SKILL_2 180400020 range_out (portée 190, distance 550) : touché=1 coups=1 dégâts=230 ÉCART
- SKILL_3 180400030 range_out (portée 1100, distance 1160) : touché=1 coups=1 dégâts=250 ÉCART
- ULTIMATE 180400050 range_out (portée 600, distance 660) : touché=1 coups=1 dégâts=380 ÉCART

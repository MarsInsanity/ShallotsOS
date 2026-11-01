# ScummVM

1. Put each game's data files in its own folder under `roms/scummvm/`.
2. Create a `<game name>.scummvm` file beside the game data. Put that game's
   ScummVM game ID on its only line:

   ```text
   game.scummvm
   ```

   Replace `game` with a readable title and the contents with its detected ID.

3. Select the `.scummvm` file in TreeFrogUI. Friendly System Names calls the
   system **ScummVM**; per-system artwork uses `scummvm.jpg`.

ScummVM needs the original game data files. The core does not include games.
Use IDs recognized by this port's supported engines; see
[TreeFrogUI FroggyScummVM](https://github.com/tzubertowski/TreeFrogUI_FroggyScummvm)
for its supported engine list and builds. The core is based on
[Synaps33's FroggyScummvm fork](https://github.com/Synaps33/FroggyScummvm).

/* NumBlocks.nwa only (NB_DIMS), included at the end of player.c: using the Nether's and the End's items and
 * blocks (1.8.8's ItemBlock for the new blocks, ItemEnderPearl, ItemEnderEye, ItemFireball,
 * BlockDragonEgg) */

/* BlockDragonEgg.teleport: to air within 15 across and 7 up or down, of 1000 tries */
static void egg_teleport(int x, int y, int z) {
  for (int i = 0; i < 1000; i++) {
    int a = rnd(16), b = rnd(16), c = rnd(8), d = rnd(8), e = rnd(16), f = rnd(16);
    int bx = x + a - b, by = y + c - d, bz = z + e - f;
    if (world_get(bx, by, bz) != B_AIR || !world_loaded(bx, by, bz)) continue;
    for (int k = 0; k < 16; k++)
      particle_add(x + rndf(), y + rndf(), z + rndf(), (rndf() - 0.5f) * 0.2f, (rndf() - 0.5f) * 0.2f, (rndf() - 0.5f) * 0.2f,
                   0x780F, 20);
    world_set(bx, by, bz, B_DRAGON_EGG);
    world_set(x, y, z, B_AIR);
    neighbours_changed(x, y, z);
    /* (it falls, as sand does) */
    while (by > 0 && replaceable(world_get(bx, by - 1, bz))) world_set(bx, by, bz, B_AIR), world_set(bx, --by, bz, B_DRAGON_EGG);
    return;
  }
}

/* ItemEnderEye.onItemUse on a frame: its eye, then if every frame of the ring has one, the portal. The
 * frames do not keep which way they face here: each way is tried (a ring with frames turned wrong still
 * opens) */
static bool frame_eye(int x, int y, int z) {
  return world_get(x, y, z) == B_END_PORTAL_FRAME_EYE;
}
static void end_ring(int x, int y, int z) {
  static const int8_t FX[4] = {0, 1, 0, -1}, FZ[4] = {-1, 0, 1, 0};   /* north, east, south, west */
  for (int f = 0; f < 4; f++) {
    int r = (f + 1) & 3;   /* rotateY */
    int j = 0, k = 0, found = 0, ok = 1;
    for (int l = -2; l <= 2; l++) {
      int bx = x + FX[r] * l, bz = z + FZ[r] * l, b = world_get(bx, y, bz);
      if (b == B_END_PORTAL_FRAME || b == B_END_PORTAL_FRAME_EYE) {
        if (b != B_END_PORTAL_FRAME_EYE) {
          ok = 0;
          break;
        }
        k = l;
        if (!found) j = l, found = 1;
      }
    }
    if (!ok || k != j + 2) continue;
    int fx = x + FX[f] * 4, fz = z + FZ[f] * 4;
    for (int i = j; i <= k && ok; i++) ok = frame_eye(fx + FX[r] * i, y, fz + FZ[r] * i);
    for (int i = j - 1; i <= k + 1 && ok; i += 4)
      for (int l = 1; l <= 3 && ok; l++) ok = frame_eye(x + FX[r] * i + FX[f] * l, y, z + FZ[r] * i + FZ[f] * l);
    if (!ok) continue;
    for (int i = j; i <= k; i++)
      for (int l = 1; l <= 3; l++) world_set(x + FX[r] * i + FX[f] * l, y, z + FZ[r] * i + FZ[f] * l, B_END_PORTAL);
    return;
  }
}

/* using an item of the Nether or the End (true: done) */
static bool dims_use(int k, bool on_block, int x, int y, int z, int b) {
  Stack *h = held();
  if (on_block && b == B_DRAGON_EGG && !pl.sneaking) {
    egg_teleport(x, y, z);
    player_swing();
    return true;
  }
  switch (k) {
    case IK_DBLOCK: {
      /* ItemBlock: stairs rising the way the player looks, a pillar along the face's axis */
      int pb = it_place[h->id - 256];
      int way = (int)floorf(pl.yaw / 90 + 0.5f) & 3;
      static const int8_t st[4] = {2, 1, 3, 0};
      if (pb == B_NETHER_BRICK_STAIRS_E || pb == B_QUARTZ_STAIRS_E) pb += st[way];
      if (pb == B_PILLAR_QUARTZ && pl.hit_face >= 0) {
        int a = pl.hit_face >> 1;
        pb += a == 2 ? 1 : a == 1 ? 2 : 0;
      }
      if (on_block && place(pb)) use_up(), player_swing();
      return true;
    }
    case IK_ENDER_PEARL:
      /* ItemEnderPearl (in Creative, nothing) */
      if (pl.mode == 1) return true;
      throw_item(I_ENDER_PEARL, 1.5f, true);
      use_up();
      player_swing();
      return true;
    case IK_ENDER_EYE: {
      if (on_block && b == B_END_PORTAL_FRAME && pl.mode != 3) {
        world_set(x, y, z, B_END_PORTAL_FRAME_EYE);
        use_up();
        for (int i = 0; i < 16; i++) particle_add(x + rndf(), y + 0.8125f, z + rndf(), 0, 0.05f, 0, 0x4208, 20);
        end_ring(x, y, z);
        return true;
      }
      if (on_block && b == B_END_PORTAL_FRAME_EYE) return true;
      /* thrown towards the nearest stronghold (in the Overworld; nowhere else) */
      int at[3];
      if (dim == DIM_OVERWORLD && gen_stronghold_near(ifloor(pl.x), ifloor(pl.z), at)) {
        throw_eye(at[0], at[1], at[2]);
        use_up();
        player_swing();
      }
      return true;
    }
    case IK_FIRE_CHARGE:
      /* ItemFireball.onItemUse: fire against the face */
      if (on_block) {
        int f = pl.hit_face, fx = x + NX[f], fy = y + NY[f], fz = z + NZ[f];
        if (world_get(fx, fy, fz) == B_AIR && fy >= 0 && fy < WORLD_H) fire_set(fx, fy, fz, 0);
        use_up();
      }
      return true;
  }
  return false;
}

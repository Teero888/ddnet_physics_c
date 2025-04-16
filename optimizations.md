# Optimizations

## Rounding Behavior

The default DDNet physics implementation uses the following rounding function:

```cpp
constexpr int round_to_int(float f)
{
    return f > 0 ? (int)(f + 0.5f) : (int)(f - 0.5f);
}
```

This function is computationally expensive, especially since DDNet employs it in performance-critical functions like [CCollision::MoveBox](https://github.com/ddnet/ddnet/blob/0832e044ab9bb51bfa44f8b5e2299e085173c41a/src/game/collision.cpp#L521) and [CCollision::IntersectLineTeleHook](https://github.com/ddnet/ddnet/blob/0832e044ab9bb51bfa44f8b5e2299e085173c41a/src/game/collision.cpp#L360).

To optimize this, we ignore negative positions entirely. **Going out of bounds is undefined behavior in this version of the DDNet physics.** Therefore, we simplify the rounding to `(int)(f + 0.5f)`. This approach is acceptable because maps can be designed to ensure positions remain within bounds.

## Tile Checks

We precompute many combinations of tile checks and store them in a flattened `uint8_t` bitmap for fast access. For example, checking if a tile is solid (i.e., `TILE_SOLID` or `TILE_NOHOOK`) is simplified to:

```c
pCollision->m_pTileInfos[pCollision->m_pWidthLookup[Ny] + Nx] & INFO_ISSOLID
```

While experimenting, [Tater](https://github.com/sjrc6/) and I tried using a width lookup instead of performing the multiplication `pCollision->m_MapData.m_Width * Ny + Nx`. According to my profiler, this is faster in some cases but not all, which is why you’ll see it used selectively throughout the codebase.

## Pickups

Pickups are typically stored in a linked list and iterated over to check if they are within reach of a character. Some maps have hundreds of pickups, leading to expensive distance checks in the default physics loop. To optimize this, we do not support moving pickups or lasers. This allows us to store pickups in a flattened array. Now, we can perform a single check to see if any pickups are nearby and, if so, check up to nine tiles for pickups.

We use a precomputed info lookup to skip the entire function if no pickups are in range:

```c
if (!(pCore->m_pCollision->m_pTileInfos[pCore->m_BlockIdx] & INFO_PICKUPNEXT))
    return;
```

First, we perform a squared distance check because the pickup hitbox is large, and in most cases, the character will be well inside it. If pickups are within the approximated range, the code looks like this:

```c
for (int dy = -1; dy <= 1; ++dy) {
    const int Idx = pCore->m_pCollision->m_pWidthLookup[iclamp(iy + dy, 0, Height - 1)];
    for (int dx = -1; dx <= 1; ++dx) {
        const SPickup Pickup = pCore->m_pCollision->m_pPickups[Idx + iclamp(ix + dx, 0, Width - 1)];
        if (Pickup.m_Type < 0)
            continue;
        const vec2 OffsetPos = vvadd(pCore->m_Pos, vec2_init(dx * 32, dy * 32));
        if (vsqdistance(pCore->m_Pos, OffsetPos) >= 48 * 48 && vdistance(pCore->m_Pos, OffsetPos) >= 48)
            continue;
        if (Pickup.m_Number > 0 && !pCore->m_pWorld->m_pSwitches[Pickup.m_Number].m_Status)
            continue;
        // ... the actual logic
    }
}
```

## Broad Checks

Broad checks determine if there is any specific tile in a rectangle between two points. To avoid a double for-loop iterating over every tile in the rectangle, we precompute every possible value and store them in a flattened array of bitmaps using `uint64_t`. See the precomputation at [collision.c:346](https://github.com/Teero888/ddnet_physics_c/blob/c73f6412b7d71d530dd9b1f6a66eb075d9a6a784/src/collision.c#L346C1-L404C4) and an example of indexing these bitmaps [here](https://github.com/Teero888/ddnet_physics_c/blob/c73f6412b7d71d530dd9b1f6a66eb075d9a6a784/src/collision.c#L710C1-L719C2).

The precomputation limits us to checking regions of 8x8 blocks. This would break if the tee or hook moves faster than 256 units per tick. The default hook speed is 80-81 units per tick, and the maximum tee speed in the x-axis is 48 units per tick (assuming default velocity ramp tunes). The y-velocity is hardcoded to a maximum of 6000 units per tick, but this is rarely reached. To maintain physics accuracy in extreme scenarios, we fall back to the normal rectangle check if the x or y velocity exceeds 256 units per tick.

## CCollision::IntersectLineTeleHook

`IntersectLineTeleHook` is one of the most performance-critical functions in DDNet. The original implementation uses an approach that interpolates between a start and end position based on the distance between them, which involves a square root and continuous linear interpolation.

Since one tile in DDNet corresponds to 32 units, we can perform a broad check first. Given that the hook is often in the air, this allows us to skip approximately 70% of the computations typically required.

The original loop code looks like this:

```cpp
float Distance = distance(Pos0, Pos1);
int End(Distance + 1);
vec2 Last = Pos0;
int dx = 0, dy = 0; // Offset for checking the "through" tile
ThroughOffset(Pos0, Pos1, &dx, &dy);
for(int i = 0; i <= End; i++)
{
    float a = i / (float)End;
    vec2 Pos = mix(Pos0, Pos1, a);
    int ix = round_to_int(Pos.x);
    int iy = round_to_int(Pos.y);
    int Index = GetPureMapIndex(Pos);
    // ... tile checking
}
```

To optimize this:

1. We use a lookup table for the square root in the distance calculation, since `End` is based on the truncated square root of the squared distance.
2. We use AVX2 instructions to calculate eight indices at a time, then fall back to a loop to check for duplicates.
3. We precompute a distance field from every unit to the nearest solid collision to perform an initial raymarching step, skipping most of the loop.

You can take a look at the optimized code [here](https://github.com/Teero888/ddnet_physics_c/blob/c73f6412b7d71d530dd9b1f6a66eb075d9a6a784/src/collision.c#L777-L862).
Currently, this code only supports the default hook fire speed, but supporting the hook speed tune is on my to-do list.

#### TODO: Explain optimizations of `MoveBox` (magic table), fire input, `wc_tick`, `vmath.h`, `get_indices`, entity struct, move restrictions, the other 10 SIMD functions, all options of the `INFO` enum, and the `WorldCore` functions.

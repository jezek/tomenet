/* $Id$ */
/* File: generate.c */

/* Purpose: Dungeon generation */

/*
 * Copyright (c) 1989 James E. Wilson, Robert A. Koeneke
 *
 * This software may be copied and distributed for educational, research, and
 * not for profit purposes provided that this copyright and statement are
 * included in all such copies.
 */

#define SERVER

#include "angband.h"

static void vault_monsters(struct worldpos *wpos, int y1, int x1, int num);

/*
 * Note that Level generation is *not* an important bottleneck,
 * though it can be annoyingly slow on older machines...  Thus
 * we emphasize "simplicity" and "correctness" over "speed".
 *
 * This entire file is only needed for generating levels.
 * This may allow smart compilers to only load it when needed.
 *
 * Consider the "v_info.txt" file for vault generation.
 *
 * In this file, we use the "special" granite and perma-wall sub-types,
 * where "basic" is normal, "inner" is inside a room, "outer" is the
 * outer wall of a room, and "solid" is the outer wall of the dungeon
 * or any walls that may not be pierced by corridors.  Thus the only
 * wall type that may be pierced by a corridor is the "outer granite"
 * type.  The "basic granite" type yields the "actual" corridors.
 *
 * Note that we use the special "solid" granite wall type to prevent
 * multiple corridors from piercing a wall in two adjacent locations,
 * which would be messy, and we use the special "outer" granite wall
 * to indicate which walls "surround" rooms, and may thus be "pierced"
 * by corridors entering or leaving the room.
 *
 * Note that a tunnel which attempts to leave a room near the "edge"
 * of the dungeon in a direction toward that edge will cause "silly"
 * wall piercings, but will have no permanently incorrect effects,
 * as long as the tunnel can *eventually* exit from another side.
 * And note that the wall may not come back into the room by the
 * hole it left through, so it must bend to the left or right and
 * then optionally re-enter the room (at least 2 grids away).  This
 * is not a problem since every room that is large enough to block
 * the passage of tunnels is also large enough to allow the tunnel
 * to pierce the room itself several times.
 *
 * Note that no two corridors may enter a room through adjacent grids,
 * they must either share an entryway or else use entryways at least
 * two grids apart.  This prevents "large" (or "silly") doorways.
 *
 * To create rooms in the dungeon, we first divide the dungeon up
 * into "blocks" of 11x11 grids each, and require that all rooms
 * occupy a rectangular group of blocks.  As long as each room type
 * reserves a sufficient number of blocks, the room building routines
 * will not need to check bounds.  Note that most of the normal rooms
 * actually only use 23x11 grids, and so reserve 33x11 grids.
 *
 * Note that the use of 11x11 blocks (instead of the old 33x11 blocks)
 * allows more variability in the horizontal placement of rooms, and
 * at the same time has the disadvantage that some rooms (two thirds
 * of the normal rooms) may be "split" by panel boundaries.  This can
 * induce a situation where a player is in a room and part of the room
 * is off the screen.  It may be annoying enough to go back to 33x11
 * blocks to prevent this visual situation.
 *
 * Note that the dungeon generation routines are much different (2.7.5)
 * and perhaps "DUN_ROOMS" should be less than 50.
 *
 * XXX XXX XXX Note that it is possible to create a room which is only
 * connected to itself, because the "tunnel generation" code allows a
 * tunnel to leave a room, wander around, and then re-enter the room.
 *
 * XXX XXX XXX Note that it is possible to create a set of rooms which
 * are only connected to other rooms in that set, since there is nothing
 * explicit in the code to prevent this from happening.  But this is less
 * likely than the "isolated room" problem, because each room attempts to
 * connect to another room, in a giant cycle, thus requiring at least two
 * bizarre occurances to create an isolated section of the dungeon.
 *
 * Note that (2.7.9) monster pits have been split into monster "nests"
 * and monster "pits".  The "nests" have a collection of monsters of a
 * given type strewn randomly around the room (jelly, animal, or undead),
 * while the "pits" have a collection of monsters of a given type placed
 * around the room in an organized manner (orc, troll, giant, dragon, or
 * demon).  Note that both "nests" and "pits" are now "level dependant",
 * and both make 16 "expensive" calls to the "get_mon_num()" function.
 *
 * Note that the cave grid flags changed in a rather drastic manner
 * for Angband 2.8.0 (and 2.7.9+), in particular, dungeon terrain
 * features, such as doors and stairs and traps and rubble and walls,
 * are all handled as a set of 64 possible "terrain features", and
 * not as "fake" objects (440-479) as in pre-2.8.0 versions.
 *
 * The 64 new "dungeon features" will also be used for "visual display"
 * but we must be careful not to allow, for example, the user to display
 * hidden traps in a different way from floors, or secret doors in a way
 * different from granite walls, or even permanent granite in a different
 * way from granite.  XXX XXX XXX
 */


/*
 * Dungeon generation values
 */
#define DUN_ROOMS	50	/* Number of rooms to attempt */
//#define DUN_UNUSUAL	150	/* Level/chance of unusual room */
#define DUN_UNUSUAL	(cfg.dun_unusual) /* Level/chance of unusual room */
#define DUN_DEST	15	/* 1/chance of having a destroyed level */

/*
 * Dungeon tunnel generation values
 */
#define DUN_TUN_RND	10	/* Chance of random direction */
#define DUN_TUN_CHG	30	/* Chance of changing direction */
#define DUN_TUN_CON	15	/* Chance of extra tunneling */
#define DUN_TUN_PEN	25	/* Chance of doors at room entrances */
#define DUN_TUN_JCT	90	/* Chance of doors at tunnel junctions */

/*
 * Dungeon streamer generation values
 */
#define DUN_STR_DEN	5	/* Density of streamers */
#define DUN_STR_RNG	2	/* Width of streamers */
#define DUN_STR_MAG	3	/* Number of magma streamers */
#define DUN_STR_MC	90	/* 1/chance of treasure per magma */
#define DUN_STR_QUA	2	/* Number of quartz streamers */
#define DUN_STR_QC	40	/* 1/chance of treasure per quartz */

/*
 * Dungeon treausre allocation values
 */
#define DUN_AMT_ROOM	9	/* Amount of objects for rooms */
#define DUN_AMT_ITEM	3	/* Amount of objects for rooms/corridors */
#define DUN_AMT_GOLD	3	/* Amount of treasure for rooms/corridors */
//#define DUN_AMT_ALTAR   1	/* Amount of altars */
#define DUN_AMT_BETWEEN 2	/* Amount of between gates */
#define DUN_AMT_FOUNTAIN 1	/* Amount of fountains */

/*
 * Hack -- Dungeon allocation "places"
 */
#define ALLOC_SET_CORR		1	/* Hallway */
#define ALLOC_SET_ROOM		2	/* Room */
#define ALLOC_SET_BOTH		3	/* Anywhere */

/*
 * Hack -- Dungeon allocation "types"
 */
#define ALLOC_TYP_RUBBLE	1	/* Rubble */
#define ALLOC_TYP_TRAP		3	/* Trap */
#define ALLOC_TYP_GOLD		4	/* Gold */
#define ALLOC_TYP_OBJECT	5	/* Object */
// #define ALLOC_TYP_FOUNT		6	/* Fountain */
#define ALLOC_TYP_ALTAR         6       /* Altar */
#define ALLOC_TYP_BETWEEN       7       /* Between */
#define ALLOC_TYP_FOUNTAIN      8       /* Fountain */



/*
 * The "size" of a "generation block" in grids
 */
#define BLOCK_HGT	11
#define BLOCK_WID	11

/*
 * Maximum numbers of rooms along each axis (currently 6x6)
 */
#define MAX_ROOMS_ROW	(MAX_HGT / BLOCK_HGT)
#define MAX_ROOMS_COL	(MAX_WID / BLOCK_WID)


/*
 * Bounds on some arrays used in the "dun_data" structure.
 * These bounds are checked, though usually this is a formality.
 */
#define CENT_MAX	100
#define DOOR_MAX	400
#define WALL_MAX	1000
#define TUNN_MAX	1800


/*
 * Maximal number of room types
 */
//#define ROOM_MAX	9
#define ROOM_MAX	13

/*
 * Maxumal 'depth' that has extra stairs;
 * also, those floors will be w/o trapped doors.
 */
#define COMFORT_PASSAGE_DEPTH 5

/*
 * ((level+TRAPPED_DOOR_BASE)/TRAPPED_DOOR_RATE) chance of generation
 */
#define TRAPPED_DOOR_RATE	400
#define TRAPPED_DOOR_BASE	30

/*
 * Below this are makeshift implementations of extra dungeon features;
 * TODO: replace them by dungeon_types etc		- Jir -
 */
/*
 * makeshift river for 3.3.x
 * pfft, still alive for 4.0.0...
 * [15,7,4,6,45,4,19,100,  1000,50]
 *
 * A player is supposed to cross 2 watery belts without avoiding them
 * by scumming;  the first by swimming, the second by flying parhaps.
 */
#define DUN_RIVER_CHANCE	15	/* The deeper, the less watery area. */
#define DUN_RIVER_REDUCE	7	/* DUN_RIVER_CHANCE / 2, maximum. */
#define DUN_STR_WAT			4
#define DUN_LAKE_TRY		6	/* how many tries to generate lake on river */
#define WATERY_CYCLE		45	/* defines 'watery belt' */
#define WATERY_RANGE		4	/* (45,4,19) = '1100-1250, 3350-3500,.. ' */
#define WATERY_OFFSET		19
#define WATERY_BELT_CHANCE	100 /* chance of river generated on watery belt */

#define DUN_MAZE_FACTOR		1000	/* depth/DUN_MAZE_FACTOR chance of maze */
#define DUN_MAZE_PERMAWALL	20	/* % of maze built of permawall */


/*
 * Those flags are mainly for vaults, quests and non-Angband dungeons...
 * but none of them implemented, qu'a faire?
 */
#if 0	// for testing
#define NO_TELEPORT_CHANCE	50
#define NO_MAGIC_CHANCE		10
#define NO_GENO_CHANCE		50
#define NO_MAP_CHANCE		30
#define NO_MAGIC_MAP_CHANCE	50
#define NO_DESTROY_CHANCE	50
#else	// normal (14% chance in total.)
#define NO_TELEPORT_CHANCE	3
#define NO_MAGIC_CHANCE		1
#define NO_GENO_CHANCE		3
#define NO_MAP_CHANCE		2
#define NO_MAGIC_MAP_CHANCE	3
#define NO_DESTROY_CHANCE	3
#endif	// 0

/*
 * Chances of a vault creating some special effects on the level, in %.
 * FORCE_FLAG bypasses this check.	[50]
 */
#define VAULT_FLAG_CHANCE	30

/*
 * Chance of 'HIVES' type vaults reproducing themselves, in %. [60]
 */
#define HIVE_CHANCE(lev)	(lev > 120 ? 60 : lev / 3 + 20)

/*
 * Borrowed stuffs from ToME
 */
#define DUN_CAVERN     30	/* chance/depth of having a cavern level */
#define DUN_CAVERN2    20	/* 1/chance extra check for cavern level */
#define EMPTY_LEVEL    15	/* 1/chance of being 'empty' (15)*/
#define DARK_EMPTY      5	/* 1/chance of arena level NOT being lit (2)*/
#define SMALL_LEVEL     3	/* 1/chance of smaller size (3)*/

/*
 * Simple structure to hold a map location
 */

typedef struct coord coord;

struct coord
{
	byte y;
	byte x;
};


/*
 * Room type information
 */

typedef struct room_data room_data;

struct room_data
{
	/* Required size in blocks */
	s16b dy1, dy2, dx1, dx2;

	/* Hack -- minimum level */
	s16b level;
};


/*
 * Structure to hold all "dungeon generation" data
 */

typedef struct dun_data dun_data;

struct dun_data
{
	/* Array of centers of rooms */
	int cent_n;
	coord cent[CENT_MAX];

	/* Array of possible door locations */
	int door_n;
	coord door[DOOR_MAX];

	/* Array of wall piercing locations */
	int wall_n;
	coord wall[WALL_MAX];

	/* Array of tunnel grids */
	int tunn_n;
	coord tunn[TUNN_MAX];

	/* Number of blocks along each axis */
	int row_rooms;
	int col_rooms;

	/* Array of which blocks are used */
	bool room_map[MAX_ROOMS_ROW][MAX_ROOMS_COL];

	/* Hack -- there is a pit/nest on this level */
	bool crowded;

	bool watery;	// this should be included in dun_level struct.
	bool no_penetr;

	dun_level *l_ptr;
	int ratio;
#if 0
	byte max_hgt;			/* Vault height */
	byte max_wid;			/* Vault width */
#endif //0
};


/*
 * Dungeon generation data -- see "cave_gen()"
 */
static dun_data *dun;

/*
 * Dungeon graphics info
 * Why the first two are byte and the rest s16b???
 */
/* ToME variables -- not implemented, but needed to have it work */
static byte feat_wall_outer = FEAT_WALL_OUTER;	/* Outer wall of rooms */
static byte feat_wall_inner = FEAT_WALL_INNER;	/* Inner wall of rooms */

static bool good_item_flag;		/* True if "Artifact" on this level */

/*
 * Array of room types (assumes 11x11 blocks)
 */
static room_data room[ROOM_MAX] =
{
	{ 0, 0, 0, 0, 0 },		/* 0 = Nothing */
	{ 0, 0, -1, 1, 1 },		/* 1 = Simple (33x11) */
	{ 0, 0, -1, 1, 1 },		/* 2 = Overlapping (33x11) */
	{ 0, 0, -1, 1, 3 },		/* 3 = Crossed (33x11) */
	{ 0, 0, -1, 1, 3 },		/* 4 = Large (33x11) */
	{ 0, 0, -1, 1, 5 },		/* 5 = Monster nest (33x11) */
	{ 0, 0, -1, 1, 5 },		/* 6 = Monster pit (33x11) */
	{ 0, 1, -1, 1, 5 },		/* 7 = Lesser vault (33x22) */
	{ -1, 2, -2, 3, 10 },	/* 8 = Greater vault (66x44) */

	{ 0, 1, 0, 1, 1 },		/* 9 = Circular rooms (22x22) */
	{ 0, 1,	-1, 1, 3 },		/* 10 = Fractal cave (42x24) */
	{ 0, 1, -1, 1, 10 },	/* 11 = Random vault (44x22) */
	{ 0, 1, 0, 1, 10 },	/* 12 = Crypts (22x22) */
};



/*
 * Always picks a correct direction
 */
static void correct_dir(int *rdir, int *cdir, int y1, int x1, int y2, int x2)
{
	/* Extract vertical and horizontal directions */
	*rdir = (y1 == y2) ? 0 : (y1 < y2) ? 1 : -1;
	*cdir = (x1 == x2) ? 0 : (x1 < x2) ? 1 : -1;

	/* Never move diagonally */
	if (*rdir && *cdir)
	{
		if (rand_int(100) < 50)
		{
			*rdir = 0;
		}
		else
		{
			*cdir = 0;
		}
	}
}


/*
 * Pick a random direction
 */
static void rand_dir(int *rdir, int *cdir)
{
	/* Pick a random direction */
	int i = rand_int(4);

	/* Extract the dy/dx components */
	*rdir = ddy_ddd[i];
	*cdir = ddx_ddd[i];
}


/*
 * Returns random co-ordinates for player starts
 */
static void new_player_spot(struct worldpos *wpos)
{
	int        y, x;

	cave_type **zcave;
	if(!(zcave=getcave(wpos))) return;

	/* Place the player */
	while (1)
	{
		/* Pick a legal spot */
		y = rand_range(1, dun->l_ptr->hgt - 2);
		x = rand_range(1, dun->l_ptr->wid - 2);

		/* Must be a "naked" floor grid */
		if (!cave_naked_bold(zcave, y, x)) continue;

		/* Refuse to start on anti-teleport grids */
		if (zcave[y][x].info & CAVE_ICKY) continue;

		/* Done */
		break;
	}

	/* Save the new grid */
	new_level_rand_y(wpos, y);
	new_level_rand_x(wpos, x);
}



/*
 * Count the number of walls adjacent to the given grid.
 *
 * Note -- Assumes "in_bounds(y, x)"
 *
 * We count only granite walls and permanent walls.
 */
static int next_to_walls(struct worldpos *wpos, int y, int x)
{
	int        k = 0;
	cave_type **zcave;
	if(!(zcave=getcave(wpos))) return(FALSE);

	if (zcave[y+1][x].feat >= FEAT_WALL_EXTRA) k++;
	if (zcave[y-1][x].feat >= FEAT_WALL_EXTRA) k++;
	if (zcave[y][x+1].feat >= FEAT_WALL_EXTRA) k++;
	if (zcave[y][x-1].feat >= FEAT_WALL_EXTRA) k++;
	return (k);
}



/*
 * Convert existing terrain type to rubble
 */
static void place_rubble(struct worldpos *wpos, int y, int x)
{
	cave_type **zcave;
	cave_type *c_ptr;
	if(!(zcave=getcave(wpos))) return;
	c_ptr=&zcave[y][x];

	/* Create rubble */
	c_ptr->feat = FEAT_RUBBLE;
}

/*
 * Create a fountain here.
 */
static void place_fountain(struct worldpos *wpos, int y, int x)
{
	cave_type **zcave;
	cave_type *c_ptr;
	int dun_level;
	c_special *cs_ptr;
	int svals[SV_POTION_LAST + SV_POTION2_LAST + 1], maxsval = 0, k;

	if(!(zcave=getcave(wpos))) return;
	dun_level = getlevel(wpos);
	c_ptr=&zcave[y][x];


	/* No fountains over traps/house doors etc */
	if (c_ptr->special) return;

	/* Place the fountain */
	if (randint(100) < 20)	/* 30 */
	{
		/* XXX Empty fountain doesn't need 'type', does it? */
		cave_set_feat(wpos, y, x, FEAT_EMPTY_FOUNTAIN);
//		c_ptr->special2 = 0;
		return;
	}

	/* List of usable svals */
	for (k = 1; k < max_k_idx; k++)
	{
		object_kind *k_ptr = &k_info[k];

		if (((k_ptr->tval == TV_POTION) || (k_ptr->tval == TV_POTION2)) &&
		    (k_ptr->level <= dun_level) && (k_ptr->flags4 & TR4_FOUNTAIN))
		{
			if (k_ptr->tval == TV_POTION2) svals[maxsval] = k_ptr->sval + SV_POTION_LAST;
			else svals[maxsval] = k_ptr->sval;
			maxsval++;
		}
	}

	if (maxsval == 0) return;
	else
	{
		/* TODO: rarity should be counted in? */
		cave_set_feat(wpos, y, x, FEAT_FOUNTAIN);
		if (!(cs_ptr=AddCS(c_ptr, CS_FOUNTAIN))) return;

		if (!maxsval || magik(20))	/* often water */
			cs_ptr->sc.fountain.type = SV_POTION_WATER;
		else cs_ptr->sc.fountain.type = svals[rand_int(maxsval)];

		cs_ptr->sc.fountain.rest = damroll(3, 4);
		cs_ptr->sc.fountain.known = FALSE;
#if 0
		cs_ptr->type = CS_TRAPS;
		c_ptr->special2 = damroll(3, 4);
#endif	// 0
	}

//	c_ptr->special = svals[rand_int(maxsval)];
}

#if 0
/*
 * Place an altar at the given location
 */
static void place_altar(int y, int x)
{
        if (magik(10))
                cave_set_feat(y, x, 164);
}
#endif	// 0



/*
 * Place a between gate at the given location
 */
static void place_between(struct worldpos *wpos, int y, int x)
{
	cave_type **zcave;
	cave_type *c_ptr, *c1_ptr;
	int gx, gy;
	c_special *cs_ptr;

	if(!(zcave=getcave(wpos))) return;
	c_ptr=&zcave[y][x];

	/* No between-gate over traps/house doors etc */
	if (c_ptr->special) return;

	while(TRUE)	/* TODO: add a counter to prevent infinite-loop */
	{
		/* Location */
		/* XXX if you call it from outside of cave-generation code,
		 * you should change here */
		gy = rand_int(dun->l_ptr->hgt);
		gx = rand_int(dun->l_ptr->wid);

		/* Require "naked" floor grid */
		if (cave_naked_bold(zcave, gy, gx))
		{
			/* Access the target grid */
			c1_ptr = &zcave[gy][gx];

			if (!c1_ptr->special) break;
		}
	}

	if (!(cs_ptr=AddCS(c_ptr, CS_BETWEEN))) return;
	cave_set_feat(wpos, y, x, FEAT_BETWEEN);
	cs_ptr->sc.between.fy = gy;
	cs_ptr->sc.between.fx = gx;

	/* XXX not 'between' gate can be generated */
	if (!(cs_ptr=AddCS(c1_ptr, CS_BETWEEN))) return;
	cave_set_feat(wpos, gy, gx, FEAT_BETWEEN);
	cs_ptr->sc.between.fy = y;
	cs_ptr->sc.between.fx = x;

#if 0
	/* Place a pair of between gates */
	cave_set_feat(wpos, y, x, FEAT_BETWEEN);
	c_ptr->special = gx + (gy << 8);
	cave_set_feat(wpos, gy, gx, FEAT_BETWEEN);
	c1_ptr->special = x + (y << 8);
#endif	// 0
}

/*
 * Convert existing terrain type to "up stairs"
 */
static void place_up_stairs(struct worldpos *wpos, int y, int x)
{
	cave_type **zcave;
	cave_type *c_ptr;
	if(!(zcave=getcave(wpos))) return;
	c_ptr=&zcave[y][x];

	/* Create up stairs */
	c_ptr->feat = FEAT_LESS;
}


/*
 * Convert existing terrain type to "down stairs"
 */
static void place_down_stairs(worldpos *wpos, int y, int x)
{
	cave_type **zcave;
	cave_type *c_ptr;
	if(!(zcave=getcave(wpos))) return;
	c_ptr=&zcave[y][x];

	/* Create down stairs */
	c_ptr->feat = FEAT_MORE;
}





/*
 * Place an up/down staircase at given location
 */
static void place_random_stairs(struct worldpos *wpos, int y, int x)
{
	cave_type **zcave;
	cave_type *c_ptr;
	if(!(zcave=getcave(wpos))) return;
	c_ptr=&zcave[y][x];
	if(!cave_clean_bold(zcave, y, x)) return;
	if(!can_go_down(wpos) && can_go_up(wpos)){
		place_up_stairs(wpos, y, x);
	}
	else if(!can_go_up(wpos) && can_go_down(wpos)){
		place_down_stairs(wpos, y, x);
	}
	else if (rand_int(100) < 50)
	{
		place_down_stairs(wpos, y, x);
	}
	else
	{
		place_up_stairs(wpos, y, x);
	}
}


/*
 * Place a locked door at the given location
 */
static void place_locked_door(struct worldpos *wpos, int y, int x)
{
	int tmp;
	cave_type **zcave;
	cave_type *c_ptr;
	if(!(zcave=getcave(wpos))) return;
	c_ptr = &zcave[y][x];

	/* Create locked door */
	c_ptr->feat = FEAT_DOOR_HEAD + randint(7);

	/* let's trap this ;) */
	if ((tmp = getlevel(wpos)) <= COMFORT_PASSAGE_DEPTH ||
		rand_int(TRAPPED_DOOR_RATE) > tmp + TRAPPED_DOOR_BASE) return;
	place_trap(wpos, y, x, 0);
}


/*
 * Place a secret door at the given location
 */
static void place_secret_door(struct worldpos *wpos, int y, int x)
{
	int tmp;
	cave_type **zcave;
	cave_type *c_ptr;
	if(!(zcave=getcave(wpos))) return;
	c_ptr = &zcave[y][x];

	/* Create secret door */
	c_ptr->feat = FEAT_SECRET;

	/* let's trap this ;) */
	if ((tmp = getlevel(wpos)) <= COMFORT_PASSAGE_DEPTH ||
		rand_int(TRAPPED_DOOR_RATE) > tmp + TRAPPED_DOOR_BASE) return;
	place_trap(wpos, y, x, 0);
}


/*
 * Place a random type of door at the given location
 */
static void place_random_door(struct worldpos *wpos, int y, int x)
{
	int tmp;
	cave_type **zcave;
	cave_type *c_ptr;
	if(!(zcave=getcave(wpos))) return;
	c_ptr = &zcave[y][x];

	/* Choose an object */
	tmp = rand_int(1000);

	/* Open doors (300/1000) */
	if (tmp < 300)
	{
		/* Create open door */
		c_ptr->feat = FEAT_OPEN;
	}

	/* Broken doors (100/1000) */
	else if (tmp < 400)
	{
		/* Create broken door */
		c_ptr->feat = FEAT_BROKEN;
	}

	/* Secret doors (200/1000) */
	else if (tmp < 600)
	{
		/* Create secret door */
		c_ptr->feat = FEAT_SECRET;
	}

	/* Closed doors (300/1000) */
	else if (tmp < 900)
	{
		/* Create closed door */
		c_ptr->feat = FEAT_DOOR_HEAD + 0x00;
	}

	/* Locked doors (99/1000) */
	else if (tmp < 999)
	{
		/* Create locked door */
		c_ptr->feat = FEAT_DOOR_HEAD + randint(7);
	}

	/* Stuck doors (1/1000) */
	else
	{
		/* Create jammed door */
		c_ptr->feat = FEAT_DOOR_HEAD + 0x08 + rand_int(8);
	}

	/* let's trap this ;) */
	if ((tmp = getlevel(wpos)) <= COMFORT_PASSAGE_DEPTH ||
		rand_int(TRAPPED_DOOR_RATE) > tmp + TRAPPED_DOOR_BASE) return;
	place_trap(wpos, y, x, 0);
}



/*
 * Places some staircases near walls
 */
static void alloc_stairs(struct worldpos *wpos, int feat, int num, int walls)
{
	int                 y, x, i, j, flag;

	cave_type		*c_ptr;
	cave_type **zcave;
	if(!(zcave=getcave(wpos))) return;

	/* Place "num" stairs */
	for (i = 0; i < num; i++)
	{
		/* Place some stairs */
		for (flag = FALSE; !flag; )
		{
			/* Try several times, then decrease "walls" */
			for (j = 0; !flag && j <= 3000; j++)
			{
				/* Pick a random grid */
				y = rand_int(dun->l_ptr->hgt - 1);
				x = rand_int(dun->l_ptr->wid - 1);

				/* Require "naked" floor grid */
				if (!cave_naked_bold(zcave, y, x)) continue;

				/* Require a certain number of adjacent walls */
				if (next_to_walls(wpos, y, x) < walls) continue;

				/* Access the grid */
				c_ptr = &zcave[y][x];

				/* Town -- must go down */
				if (!can_go_up(wpos))
				{
					/* Clear previous contents, add down stairs */
					c_ptr->feat = FEAT_MORE;
					if(!istown(wpos)){
						new_level_up_y(wpos,y);
						new_level_up_x(wpos,x);
					}
				}

				/* Quest -- must go up */
				else if (is_quest(wpos) || !can_go_down(wpos))
				{
					/* Clear previous contents, add up stairs */
					c_ptr->feat = FEAT_LESS;

					/* Set this to be the starting location for people going down */
					new_level_down_y(wpos,y);
					new_level_down_x(wpos,x);
				}

				/* Requested type */
				else
				{
					/* Clear previous contents, add stairs */
					c_ptr->feat = feat;

					if (feat == FEAT_LESS)
					{
						/* Set this to be the starting location for people going down */
						new_level_down_y(wpos, y);
						new_level_down_x(wpos, x);
					}
					if (feat == FEAT_MORE)
					{
						/* Set this to be the starting location for people going up */
						new_level_up_y(wpos, y);
						new_level_up_x(wpos, x);
					}
				}

				/* All done */
				flag = TRUE;
			}

			/* Require fewer walls */
			if (walls) walls--;
		}
	}
}




/*
 * Allocates some objects (using "place" and "type")
 */
static void alloc_object(struct worldpos *wpos, int set, int typ, int num)
{
	int y, x, k;
	int try = 1000;
	cave_type **zcave;
	if(!(zcave=getcave(wpos))) return;

	/* Place some objects */
	for (k = 0; k < num; k++)
	{
		/* Pick a "legal" spot */
		while (TRUE)
		{
			bool room;

			if (!--try)
			{
#if DEBUG_LEVEL > 1
				s_printf("alloc_object failed!\n");
#endif	// DEBUG_LEVEL
				return;
			}

			/* Location */
			y = rand_int(dun->l_ptr->hgt);
			x = rand_int(dun->l_ptr->wid);

			/* Require "naked" floor grid */
			if (!cave_naked_bold(zcave, y, x)) continue;

			/* Hack -- avoid doors */
			if (typ != ALLOC_TYP_TRAP &&
				f_info[zcave[y][x].feat].flags1 & FF1_DOOR)
				continue;

			/* Check for "room" */
			room = (zcave[y][x].info & CAVE_ROOM) ? TRUE : FALSE;

			/* Require corridor? */
			if ((set == ALLOC_SET_CORR) && room) continue;

			/* Require room? */
			if ((set == ALLOC_SET_ROOM) && !room) continue;

			/* Accept it */
			break;
		}

		/* Place something */
		switch (typ)
		{
			case ALLOC_TYP_RUBBLE:
			{
				place_rubble(wpos, y, x);
				break;
			}

			case ALLOC_TYP_TRAP:
			{
				place_trap(wpos, y, x, 0);
				break;
			}

			case ALLOC_TYP_GOLD:
			{
				place_gold(wpos, y, x);
				/* hack -- trap can be hidden under gold */
				if (rand_int(100) < 3) place_trap(wpos, y, x, 0);
				break;
			}

			case ALLOC_TYP_OBJECT:
			{
				place_object(wpos, y, x, FALSE, FALSE, default_obj_theme);
				/* hack -- trap can be hidden under an item */
				if (rand_int(100) < 2) place_trap(wpos, y, x, 0);
				break;
			}

#if 0
			case ALLOC_TYP_ALTAR:
			{
				place_altar(wpos, y, x);
				break;
			}
#endif	// 0

			case ALLOC_TYP_BETWEEN:
			{
				place_between(wpos, y, x);
				break;
			}

//			case ALLOC_TYP_FOUNT:
			case ALLOC_TYP_FOUNTAIN:
			{
				place_fountain(wpos, y, x);
				break;
			}
		}
	}
}

/*
 * Helper function for "monster nest (undead)"
 */
static bool vault_aux_aquatic(int r_idx)
{
	monster_race *r_ptr = &r_info[r_idx];

	/* Decline unique monsters */
	if (r_ptr->flags1 & RF1_UNIQUE) return (FALSE);

	/* Require Aquatique */
	if (!(r_ptr->flags7 & RF7_AQUATIC)) return (FALSE);

	/* Okay */
	return (TRUE);
}

#if 0
static void aqua_monsters(struct worldpos *wpos, int y1, int x1, int num)
{
	get_mon_num_hook = vault_aux_aquatic;

	/* Prepare allocation table */
	get_mon_num_prep();

	vault_monsters(wpos, y1, x1, num)

	/* Remove restriction */
	get_mon_num_hook = dungeon_aux;

	/* Prepare allocation table */
	get_mon_num_prep();
}
#endif	// 0

/*
 * Places "streamers" of rock through dungeon
 *
 * Note that their are actually six different terrain features used
 * to represent streamers.  Three each of magma and quartz, one for
 * basic vein, one with hidden gold, and one with known gold.  The
 * hidden gold types are currently unused.
 */
static void build_streamer(struct worldpos *wpos, int feat, int chance, bool pierce)
{
	int		i, tx, ty;
	int		y, x, dir;
	cave_type *c_ptr;

	cave_type **zcave;
	if(!(zcave=getcave(wpos))) return;

	/* Hack -- Choose starting point */
	while (1)
	{
		y = rand_spread(dun->l_ptr->hgt / 2, 10);
		x = rand_spread(dun->l_ptr->wid / 2, 15);

		if (!in_bounds4(dun->l_ptr, y, x)) continue;
		break;
	}

	/* Choose a random compass direction */
	dir = ddd[rand_int(8)];

	/* Place streamer into dungeon */
	while (TRUE)
	{
		/* One grid per density */
		for (i = 0; i < DUN_STR_DEN; i++)
		{
			int d = DUN_STR_RNG;

			/* Pick a nearby grid */
			while (1)
			{
				ty = rand_spread(y, d);
				tx = rand_spread(x, d);
//				if (!in_bounds2(wpos, ty, tx)) continue;
				if (!in_bounds4(dun->l_ptr, ty, tx)) continue;
				break;
			}

			/* Access the grid */
			c_ptr = &zcave[ty][tx];

			/* Only convert "granite" walls */
			if (pierce)
			{
				if (cave_perma_bold(zcave, ty, tx)) continue;
			}
			else
			{
				if (c_ptr->feat < FEAT_WALL_EXTRA) continue;
				if (c_ptr->feat > FEAT_WALL_SOLID) continue;
			}

			if (dun->no_penetr && c_ptr->info & CAVE_ICKY) continue;

			/* Clear previous contents, add proper vein type */
			c_ptr->feat = feat;

			/* Hack -- Add some (known) treasure */
			if (chance && rand_int(chance) == 0) c_ptr->feat += 0x04;
		}

		/* Hack^2 - place watery monsters */
		if (feat == FEAT_DEEP_WATER && magik(2)) vault_monsters(wpos, y, x, 1);

		/* Advance the streamer */
		y += ddy[dir];
		x += ddx[dir];


		/* Quit before leaving the dungeon */
//		if (!in_bounds(y, x)) break;
		if (!in_bounds4(dun->l_ptr, y, x)) break;
	}
}


/*
 * Build an underground lake 
 */
static void lake_level(struct worldpos *wpos)
{
	int y1, x1, y, x, k, t, n, rad;
	int h1, h2, h3, h4;
	bool distort = FALSE;
	cave_type *c_ptr;

	cave_type **zcave;
	if(!(zcave=getcave(wpos))) return;

	/* Drop a few epi-centers (usually about two) */
	for (n = 0; n < DUN_LAKE_TRY; n++)
	{
		/* Pick an epi-center */
		y1 = rand_range(5, dun->l_ptr->hgt - 6);
		x1 = rand_range(5, dun->l_ptr->wid - 6);

		/* Access the grid */
		c_ptr = &zcave[y1][x1];

		if (c_ptr->feat != FEAT_DEEP_WATER) continue;

		rad = rand_range(8, 18);
		distort = magik(50 - rad * 2);

		if (distort)
		{
			/* Make a random metric */
			h1 = randint(32) - 16;
			h2 = randint(16);
			h3 = randint(32);
			h4 = randint(32) - 16;
		}

		/* Big area of affect */
		for (y = (y1 - rad); y <= (y1 + rad); y++)
		{
			for (x = (x1 - rad); x <= (x1 + rad); x++)
			{
				/* Skip illegal grids */
				if (!in_bounds(y, x)) continue;

				/* Extract the distance */
				k = distort ? dist2(y1, x1, y, x, h1, h2, h3, h4) :
					distance(y1, x1, y, x);

				/* Stay in the circle of death */
				if (k > rad) continue;

				/* Delete the monster (if any) */
//				delete_monster(wpos, y, x);

				/* Destroy valid grids */
				if (cave_valid_bold(zcave, y, x))
				{
					/* Delete the object (if any) */
//					delete_object(wpos, y, x);

					/* Access the grid */
					c_ptr = &zcave[y][x];

					/* Wall (or floor) type */
					t = rand_int(200);

					/* Water */
					if (t < 174)
					{
						/* Create granite wall */
						c_ptr->feat = FEAT_DEEP_WATER;
					}
					else if (t < 180)
					{
						/* Create dirt */
						c_ptr->feat = FEAT_DIRT;
					}
					else if (t < 182)
					{
						/* Create dirt */
						c_ptr->feat = FEAT_MUD;
					}

					if (t < 2) vault_monsters(wpos, y, x, 2);

					/* No longer part of a room or vault */
					c_ptr->info &= ~(CAVE_ROOM | CAVE_ICKY);

					/* No longer illuminated or known */
					c_ptr->info &= ~CAVE_GLOW;
				}
			}
		}
		break;
	}
}


/*
 * Build a destroyed level
 */
static void destroy_level(struct worldpos *wpos)
{
	int y1, x1, y, x, k, t, n;
	cave_type *c_ptr;

	cave_type **zcave;
	if(!(zcave=getcave(wpos))) return;

	/* Drop a few epi-centers (usually about two) */
	for (n = 0; n < randint(5); n++)
	{
		/* Pick an epi-center */
		y1 = rand_range(5, dun->l_ptr->hgt - 6);
		x1 = rand_range(5, dun->l_ptr->wid - 6);

		/* Big area of affect */
		for (y = (y1 - 15); y <= (y1 + 15); y++)
		{
			for (x = (x1 - 15); x <= (x1 + 15); x++)
			{
				/* Skip illegal grids */
				if (!in_bounds(y, x)) continue;

				/* Extract the distance */
				k = distance(y1, x1, y, x);

				/* Stay in the circle of death */
				if (k >= 16) continue;

				/* Delete the monster (if any) */
				delete_monster(wpos, y, x);

				/* Destroy valid grids */
				if (cave_valid_bold(zcave, y, x))
				{
					/* Delete the object (if any) */
					delete_object(wpos, y, x);

					/* Access the grid */
					c_ptr = &zcave[y][x];

					/* Wall (or floor) type */
					t = rand_int(200);

					/* Granite */
					if (t < 20)
					{
						/* Create granite wall */
						c_ptr->feat = FEAT_WALL_EXTRA;
					}

					/* Quartz */
					else if (t < 70)
					{
						/* Create quartz vein */
						c_ptr->feat = FEAT_QUARTZ;
					}

					/* Magma */
					else if (t < 100)
					{
						/* Create magma vein */
						c_ptr->feat = FEAT_MAGMA;
					}

					/* Floor */
					else
					{
						/* Create floor */
						c_ptr->feat = FEAT_FLOOR;
					}

					/* No longer part of a room or vault */
					c_ptr->info &= ~(CAVE_ROOM | CAVE_ICKY);

					/* No longer illuminated or known */
					c_ptr->info &= ~CAVE_GLOW;
				}
			}
		}
	}
}



/*
 * Create up to "num" objects near the given coordinates
 * Only really called by some of the "vault" routines.
 */
static void vault_objects(struct worldpos *wpos, int y, int x, int num)
{
	int        i, j, k;
	cave_type **zcave;
	if(!(zcave=getcave(wpos))) return;

	/* Attempt to place 'num' objects */
	for (; num > 0; --num)
	{
		/* Try up to 11 spots looking for empty space */
		for (i = 0; i < 11; ++i)
		{
			/* Pick a random location */
			while (1)
			{
				j = rand_spread(y, 2);
				k = rand_spread(x, 3);
				if (!in_bounds(j, k)) continue;
				break;
			}

			/* Require "clean" floor space */
			if (!cave_clean_bold(zcave, j, k)) continue;

			/* Place an item */
			if (rand_int(100) < 75)
			{
				place_object(wpos, j, k, FALSE, FALSE, default_obj_theme);
			}

			/* Place gold */
			else
			{
				place_gold(wpos, j, k);
			}

			/* Placement accomplished */
			break;
		}
	}
}


/*
 * Place a trap with a given displacement of point
 */
static void vault_trap_aux(struct worldpos *wpos, int y, int x, int yd, int xd)
{
	int		count, y1, x1;
	cave_type **zcave;
	if(!(zcave=getcave(wpos))) return;

	/* Place traps */
	for (count = 0; count <= 5; count++)
	{
		/* Get a location */
		while (1)
		{
			y1 = rand_spread(y, yd);
			x1 = rand_spread(x, xd);
			if (!in_bounds(y1, x1)) continue;
			break;
		}

		/* Require "naked" floor grids */
//		if (!cave_naked_bold(zcave, y1, x1)) continue;

		/* Place the trap */
		place_trap(wpos, y1, x1, 0);

		/* Done */
		break;
	}
}


/*
 * Place some traps with a given displacement of given location
 */
static void vault_traps(struct worldpos *wpos, int y, int x, int yd, int xd, int num)
{
	int i;

	for (i = 0; i < num; i++)
	{
		vault_trap_aux(wpos, y, x, yd, xd);
	}
}


/*
 * Hack -- Place some sleeping monsters near the given location
 */
static void vault_monsters(struct worldpos *wpos, int y1, int x1, int num)
{
	int          k, i, y, x;
	cave_type **zcave;
	if(!(zcave=getcave(wpos))) return;

	/* Try to summon "num" monsters "near" the given location */
	for (k = 0; k < num; k++)
	{
		/* Try nine locations */
		for (i = 0; i < 9; i++)
		{
			int d = 1;

			/* Pick a nearby location */
			scatter(wpos, &y, &x, y1, x1, d, 0);

			/* Require "empty" floor grids */
			if (!cave_empty_bold(zcave, y, x)) continue;

			/* Place the monster (allow groups) */
			monster_level = getlevel(wpos) + 2;
			(void)place_monster(wpos, y, x, TRUE, TRUE);
			monster_level = getlevel(wpos);
		}
	}
}


/* XXX XXX Here begins a big lump of ToME cake	- Jir - */
/* XXX part I -- generic functions */


/*
 * Place a cave filler at (y, x)
 */
static void place_filler(worldpos *wpos, int y, int x)
{
//	cave_set_feat(wpos, y, x, fill_type[rand_int(100)]);
	cave_set_feat(wpos, y, x, FEAT_WALL_EXTRA);
}

/*
 * Place floor terrain at (y, x) according to dungeon info
 */
void place_floor(worldpos *wpos, int y, int x)
{
//	cave_set_feat(wpos, y, x, floor_type[rand_int(100)]);
	cave_set_feat(wpos, y, x, dun->watery ? FEAT_SHAL_WATER : FEAT_FLOOR);
	cave_set_feat(wpos, y, x, FEAT_FLOOR);
}

/*
 * Place floor terrain at (y, x) according to dungeon info
 */
void place_floor_respectedly(worldpos *wpos, int y, int x)
{
	cave_type **zcave;
	if(!(zcave=getcave(wpos))) return;

//	cave_set_feat(wpos, y, x, floor_type[rand_int(100)]);
//	cave_set_feat(wpos, y, x, dun->watery ? FEAT_WATER : FEAT_FLOOR);

	if (dun->no_penetr && zcave[y][x].info & CAVE_ICKY) return;
	cave_set_feat(wpos, y, x, FEAT_FLOOR);
}

/*
 * Function that sees if a square is a floor (Includes range checking)
 */
static bool get_is_floor(worldpos *wpos, int x, int y)
{
	cave_type **zcave;
	if(!(zcave=getcave(wpos))) return;

	/* Out of bounds */
	if (!in_bounds(y, x)) return (FALSE);	/* XXX */

	/* Do the real check: */
//	if (f_info[cave[y][x].feat].flags1 & FF1_FLOOR) return (TRUE);
	if (zcave[y][x].feat == FEAT_FLOOR ||
		zcave[y][x].feat == FEAT_DEEP_WATER) return (TRUE);

	return (FALSE);
}

/*
 * Tunnel around a room if it will cut off part of a cave system
 */
static void check_room_boundary(worldpos *wpos, int x1, int y1, int x2, int y2)
{
	int count, x, y;
	bool old_is_floor, new_is_floor;

	cave_type **zcave;
	if(!(zcave=getcave(wpos))) return;

	/* Avoid doing this in irrelevant places -- pelpel */
//	if (!(d_info[dungeon_type].flags1 & DF1_CAVERN)) return;

	/* Initialize */
	count = 0;

	old_is_floor = get_is_floor(wpos, x1 - 1, y1);

	/*
	 * Count the number of floor-wall boundaries around the room
	 * Note: diagonal squares are ignored since the player can move diagonally
	 * to bypass these if needed.
	 */

	/* Above the top boundary */
	for (x = x1; x <= x2; x++)
	{
		new_is_floor = get_is_floor(wpos, x, y1 - 1);

		/* increment counter if they are different */
		if (new_is_floor != old_is_floor) count++;

		old_is_floor = new_is_floor;
	}

	/* Right boundary */
	for (y = y1; y <= y2; y++)
	{
		new_is_floor = get_is_floor(wpos, x2 + 1, y);

		/* increment counter if they are different */
		if (new_is_floor != old_is_floor) count++;

		old_is_floor = new_is_floor;
	}

	/* Bottom boundary*/
	for (x = x2; x >= x1; x--)
	{
		new_is_floor = get_is_floor(wpos, x, y2 + 1);

		/* Increment counter if they are different */
		if (new_is_floor != old_is_floor) count++;

		old_is_floor = new_is_floor;
	}

	/* Left boundary */
	for (y = y2; y >= y1; y--)
	{
		new_is_floor = get_is_floor(wpos, x1 - 1, y);

		/* Increment counter if they are different */
		if (new_is_floor != old_is_floor) count++;

		old_is_floor = new_is_floor;
	}


	/* If all the same, or only one connection exit */
	if ((count == 0) || (count == 2)) return;


	/* Tunnel around the room so to prevent problems with caves */
	for (y = y1; y <= y2; y++)
	{
		for (x = x1; x <= x2; x++)
		{
			if (in_bounds(y, x)) place_floor(wpos, y, x);	// XXX
		}
	}
}


/*
 * Allocate the space needed by a room in the room_map array.
 *
 * x, y represent the size of the room (0...x-1) by (0...y-1).
 * crowded is used to denote a monset nest.
 * by0, bx0 are the positions in the room_map array given to the build_type'x'
 * function.
 * xx, yy are the returned center of the allocated room in coordinates for
 * cave.feat and cave.info etc.
 */
bool room_alloc(worldpos *wpos, int x, int y, bool crowded, int by0, int bx0, int *xx, int *yy)
{
	int temp, bx1, bx2, by1, by2, by, bx;


	/* Calculate number of room_map squares to allocate */

	/* Total number along width */
	temp = ((x - 1) / BLOCK_WID) + 1;

	/* Ending block */
	bx2 = temp / 2 + bx0;

	/* Starting block (Note: rounding taken care of here) */
	bx1 = bx2 + 1 - temp;


	/* Total number along height */
	temp = ((y - 1) / BLOCK_HGT) + 1;

	/* Ending block */
	by2 = temp / 2 + by0;

	/* Starting block */
	by1 = by2 + 1 - temp;


	/* Never run off the screen */
	if ((by1 < 0) || (by2 >= dun->row_rooms)) return (FALSE);
	if ((bx1 < 0) || (bx2 >= dun->col_rooms)) return (FALSE);

	/* Verify open space */
	for (by = by1; by <= by2; by++)
	{
		for (bx = bx1; bx <= bx2; bx++)
		{
			if (dun->room_map[by][bx]) return (FALSE);
		}
	}

	/*
	 * It is *extremely* important that the following calculation
	 * be *exactly* correct to prevent memory errors XXX XXX XXX
	 */

	/* Acquire the location of the room */
	*yy = ((by1 + by2 + 1) * BLOCK_HGT) / 2;
	*xx = ((bx1 + bx2 + 1) * BLOCK_WID) / 2;

	/* Save the room location */
	if (dun->cent_n < CENT_MAX)
	{
		dun->cent[dun->cent_n].y = *yy;
		dun->cent[dun->cent_n].x = *xx;
		dun->cent_n++;
	}

	/* Reserve some blocks */
	for (by = by1; by <= by2; by++)
	{
		for (bx = bx1; bx <= bx2; bx++)
		{
			dun->room_map[by][bx] = TRUE;
		}
	}

	/* Count "crowded" rooms */
	if (crowded) dun->crowded = TRUE;

	/*
	 * Hack -- See if room will cut off a cavern.
	 * If so, fix by tunneling outside the room in such a way as
	 * to conect the caves.
	 */
	check_room_boundary(wpos, *xx - x / 2 - 1, *yy - y / 2 - 1,
	                    *xx + x / 2 + 1, *yy + y / 2 + 1);

	/* Success */
	return (TRUE);
}


/*
 * The following functions create a rectangle (e.g. outer wall of rooms)
 */
void build_rectangle(worldpos *wpos, int y1, int x1, int y2, int x2, int feat, int info)
{
	int y, x;

	cave_type **zcave;
	if(!(zcave=getcave(wpos))) return;

	/* Top and bottom boundaries */
	for (x = x1; x <= x2; x++)
	{
		cave_set_feat(wpos, y1, x, feat);
		zcave[y1][x].info |= (info);

		cave_set_feat(wpos, y2, x, feat);
		zcave[y2][x].info |= (info);
	}

	/* Top and bottom boundaries */
	for (y = y1; y <= y2; y++)
	{
		cave_set_feat(wpos, y, x1, feat);
		zcave[y][x1].info |= (info);

		cave_set_feat(wpos, y, x2, feat);
		zcave[y][x2].info |= (info);
	}
}




/*
 * Room building routines.
 *
 * Six basic room types:
 *   1 -- normal
 *   2 -- overlapping
 *   3 -- cross shaped
 *   4 -- large room with features
 *   5 -- monster nests
 *   6 -- monster pits
 *   7 -- simple vaults
 *   8 -- greater vaults
 */


/*
 * Type 1 -- normal rectangular rooms
 */
//static void build_type1(struct worldpos *wpos, int yval, int xval)
static void build_type1(struct worldpos *wpos, int by0, int bx0)
{
	int y, x = 1, y2, x2, yval, xval;
	int y1, x1, xsize, ysize;

	bool		light;
	cave_type *c_ptr;

	cave_type **zcave;
	if(!(zcave=getcave(wpos))) return;

	/* Choose lite or dark */
	light = (getlevel(wpos) <= randint(25)) || magik(2);

#if 0
	/* Pick a room size */
	y1 = yval - randint(4);
	y2 = yval + randint(3);
	x1 = xval - randint(11);
	x2 = xval + randint(11);
#endif	// 0

	/* Pick a room size */
	y1 = randint(4);
	x1 = randint(11);
	y2 = randint(3);
	x2 = randint(11);

	xsize = x1 + x2 + 1;
	ysize = y1 + y2 + 1;

	/* Try to allocate space for room.  If fails, exit */
	if (!room_alloc(wpos, xsize + 2, ysize + 2, FALSE, by0, bx0, &xval, &yval)) return;

	/* Get corner values */
	y1 = yval - ysize / 2;
	x1 = xval - xsize / 2;
	y2 = yval + (ysize + 1) / 2;
	x2 = xval + (xsize + 1) / 2;


	/* Place a full floor under the room */
	for (y = y1 - 1; y <= y2 + 1; y++)
	{
		for (x = x1 - 1; x <= x2 + 1; x++)
		{
			c_ptr = &zcave[y][x];
			c_ptr->feat = FEAT_FLOOR;
			c_ptr->info |= CAVE_ROOM;
			if (light) c_ptr->info |= CAVE_GLOW;
		}
	}

	/* Walls around the room */
	for (y = y1 - 1; y <= y2 + 1; y++)
	{
		c_ptr = &zcave[y][x1-1];
		c_ptr->feat = FEAT_WALL_OUTER;
		c_ptr = &zcave[y][x2+1];
		c_ptr->feat = FEAT_WALL_OUTER;
	}
	for (x = x1 - 1; x <= x2 + 1; x++)
	{
		c_ptr = &zcave[y1-1][x];
		c_ptr->feat = FEAT_WALL_OUTER;
		c_ptr = &zcave[y2+1][x];
		c_ptr->feat = FEAT_WALL_OUTER;
	}


	/* Hack -- Occasional pillar room */
	if (rand_int(20) == 0)
	{
		for (y = y1; y <= y2; y += 2)
		{
			for (x = x1; x <= x2; x += 2)
			{
				c_ptr = &zcave[y][x];
				c_ptr->feat = FEAT_WALL_INNER;
			}
		}
	}

	/* Hack -- Occasional ragged-edge room */
	else if (rand_int(50) == 0)
	{
		for (y = y1 + 2; y <= y2 - 2; y += 2)
		{
			c_ptr = &zcave[y][x1];
			c_ptr->feat = FEAT_WALL_INNER;
			c_ptr = &zcave[y][x2];
			c_ptr->feat = FEAT_WALL_INNER;
		}
		for (x = x1 + 2; x <= x2 - 2; x += 2)
		{
			c_ptr = &zcave[y1][x];
			c_ptr->feat = FEAT_WALL_INNER;
			c_ptr = &zcave[y2][x];
			c_ptr->feat = FEAT_WALL_INNER;
		}
	}
}


/*
 * Type 2 -- Overlapping rectangular rooms
 */
//static void build_type2(struct worldpos *wpos, int yval, int xval)
static void build_type2(struct worldpos *wpos, int by0, int bx0)
{
	int y, x, yval, xval;
	int			y1a, x1a, y2a, x2a;
	int			y1b, x1b, y2b, x2b;

	bool		light;
	cave_type *c_ptr;

	cave_type **zcave;
	if(!(zcave=getcave(wpos))) return;

	/* Choose lite or dark */
	light = (getlevel(wpos) <= randint(25));

	/* Try to allocate space for room.  If fails, exit */
	if (!room_alloc(wpos, 25, 11, FALSE, by0, bx0, &xval, &yval))return;


	/* Determine extents of the first room */
	y1a = yval - randint(4);
	y2a = yval + randint(3);
	x1a = xval - randint(11);
	x2a = xval + randint(10);

	/* Determine extents of the second room */
	y1b = yval - randint(3);
	y2b = yval + randint(4);
	x1b = xval - randint(10);
	x2b = xval + randint(11);

	/* Place a full floor for room "a" */
	for (y = y1a - 1; y <= y2a + 1; y++)
	{
		for (x = x1a - 1; x <= x2a + 1; x++)
		{
			c_ptr = &zcave[y][x];
			c_ptr->feat = FEAT_FLOOR;
			c_ptr->info |= CAVE_ROOM;
			if (light) c_ptr->info |= CAVE_GLOW;
		}
	}

	/* Place a full floor for room "b" */
	for (y = y1b - 1; y <= y2b + 1; y++)
	{
		for (x = x1b - 1; x <= x2b + 1; x++)
		{
			c_ptr = &zcave[y][x];
			c_ptr->feat = FEAT_FLOOR;
			c_ptr->info |= CAVE_ROOM;
			if (light) c_ptr->info |= CAVE_GLOW;
		}
	}


	/* Place the walls around room "a" */
	for (y = y1a - 1; y <= y2a + 1; y++)
	{
		c_ptr = &zcave[y][x1a-1];
		c_ptr->feat = FEAT_WALL_OUTER;
		c_ptr = &zcave[y][x2a+1];
		c_ptr->feat = FEAT_WALL_OUTER;
	}
	for (x = x1a - 1; x <= x2a + 1; x++)
	{
		c_ptr = &zcave[y1a-1][x];
		c_ptr->feat = FEAT_WALL_OUTER;
		c_ptr = &zcave[y2a+1][x];
		c_ptr->feat = FEAT_WALL_OUTER;
	}

	/* Place the walls around room "b" */
	for (y = y1b - 1; y <= y2b + 1; y++)
	{
		c_ptr = &zcave[y][x1b-1];
		c_ptr->feat = FEAT_WALL_OUTER;
		c_ptr = &zcave[y][x2b+1];
		c_ptr->feat = FEAT_WALL_OUTER;
	}
	for (x = x1b - 1; x <= x2b + 1; x++)
	{
		c_ptr = &zcave[y1b-1][x];
		c_ptr->feat = FEAT_WALL_OUTER;
		c_ptr = &zcave[y2b+1][x];
		c_ptr->feat = FEAT_WALL_OUTER;
	}



	/* Replace the floor for room "a" */
	for (y = y1a; y <= y2a; y++)
	{
		for (x = x1a; x <= x2a; x++)
		{
			c_ptr = &zcave[y][x];
			c_ptr->feat = FEAT_FLOOR;
		}
	}

	/* Replace the floor for room "b" */
	for (y = y1b; y <= y2b; y++)
	{
		for (x = x1b; x <= x2b; x++)
		{
			c_ptr = &zcave[y][x];
			c_ptr->feat = FEAT_FLOOR;
		}
	}
}



/*
 * Type 3 -- Cross shaped rooms
 *
 * Builds a room at a row, column coordinate
 *
 * Room "a" runs north/south, and Room "b" runs east/east
 * So the "central pillar" runs from x1a,y1b to x2a,y2b.
 *
 * Note that currently, the "center" is always 3x3, but I think that
 * the code below will work (with "bounds checking") for 5x5, or even
 * for unsymetric values like 4x3 or 5x3 or 3x4 or 3x5, or even larger.
 */
//static void build_type3(struct worldpos *wpos, int yval, int xval)
static void build_type3(struct worldpos *wpos, int by0, int bx0)
{
	int			y, x, dy, dx, wy, wx;
	int			y1a, x1a, y2a, x2a;
	int			y1b, x1b, y2b, x2b;
	int yval, xval;

	bool		light;
	cave_type *c_ptr;

	cave_type **zcave;
	if(!(zcave=getcave(wpos))) return;

	/* Try to allocate space for room.  If fails, exit */
	if (!room_alloc(wpos, 25, 11, FALSE, by0, bx0, &xval, &yval)) return;

	/* Choose lite or dark */
	light = (getlevel(wpos) <= randint(25));


	/* For now, always 3x3 */
	wx = wy = 1;

	/* Pick max vertical size (at most 4) */
	dy = rand_range(3, 4);

	/* Pick max horizontal size (at most 15) */
	dx = rand_range(3, 11);


	/* Determine extents of the north/south room */
	y1a = yval - dy;
	y2a = yval + dy;
	x1a = xval - wx;
	x2a = xval + wx;

	/* Determine extents of the east/west room */
	y1b = yval - wy;
	y2b = yval + wy;
	x1b = xval - dx;
	x2b = xval + dx;


	/* Place a full floor for room "a" */
	for (y = y1a - 1; y <= y2a + 1; y++)
	{
		for (x = x1a - 1; x <= x2a + 1; x++)
		{
			c_ptr = &zcave[y][x];
			c_ptr->feat = FEAT_FLOOR;
			c_ptr->info |= CAVE_ROOM;
			if (light) c_ptr->info |= CAVE_GLOW;
		}
	}

	/* Place a full floor for room "b" */
	for (y = y1b - 1; y <= y2b + 1; y++)
	{
		for (x = x1b - 1; x <= x2b + 1; x++)
		{
			c_ptr = &zcave[y][x];
			c_ptr->feat = FEAT_FLOOR;
			c_ptr->info |= CAVE_ROOM;
			if (light) c_ptr->info |= CAVE_GLOW;
		}
	}


	/* Place the walls around room "a" */
	for (y = y1a - 1; y <= y2a + 1; y++)
	{
		c_ptr = &zcave[y][x1a-1];
		c_ptr->feat = FEAT_WALL_OUTER;
		c_ptr = &zcave[y][x2a+1];
		c_ptr->feat = FEAT_WALL_OUTER;
	}
	for (x = x1a - 1; x <= x2a + 1; x++)
	{
		c_ptr = &zcave[y1a-1][x];
		c_ptr->feat = FEAT_WALL_OUTER;
		c_ptr = &zcave[y2a+1][x];
		c_ptr->feat = FEAT_WALL_OUTER;
	}

	/* Place the walls around room "b" */
	for (y = y1b - 1; y <= y2b + 1; y++)
	{
		c_ptr = &zcave[y][x1b-1];
		c_ptr->feat = FEAT_WALL_OUTER;
		c_ptr = &zcave[y][x2b+1];
		c_ptr->feat = FEAT_WALL_OUTER;
	}
	for (x = x1b - 1; x <= x2b + 1; x++)
	{
		c_ptr = &zcave[y1b-1][x];
		c_ptr->feat = FEAT_WALL_OUTER;
		c_ptr = &zcave[y2b+1][x];
		c_ptr->feat = FEAT_WALL_OUTER;
	}


	/* Replace the floor for room "a" */
	for (y = y1a; y <= y2a; y++)
	{
		for (x = x1a; x <= x2a; x++)
		{
			c_ptr = &zcave[y][x];
			c_ptr->feat = FEAT_FLOOR;
		}
	}

	/* Replace the floor for room "b" */
	for (y = y1b; y <= y2b; y++)
	{
		for (x = x1b; x <= x2b; x++)
		{
			c_ptr = &zcave[y][x];
			c_ptr->feat = FEAT_FLOOR;
		}
	}



	/* Special features (3/4) */
	switch (rand_int(4))
	{
		/* Large solid middle pillar */
		case 1:
		for (y = y1b; y <= y2b; y++)
		{
			for (x = x1a; x <= x2a; x++)
			{
				c_ptr = &zcave[y][x];
				c_ptr->feat = FEAT_WALL_INNER;
			}
		}
		break;

		/* Inner treasure vault */
		case 2:

		/* Build the vault */
		for (y = y1b; y <= y2b; y++)
		{
			c_ptr = &zcave[y][x1a];
			c_ptr->feat = FEAT_WALL_INNER;
			c_ptr = &zcave[y][x2a];
			c_ptr->feat = FEAT_WALL_INNER;
		}
		for (x = x1a; x <= x2a; x++)
		{
			c_ptr = &zcave[y1b][x];
			c_ptr->feat = FEAT_WALL_INNER;
			c_ptr = &zcave[y2b][x];
			c_ptr->feat = FEAT_WALL_INNER;
		}

		/* Place a secret door on the inner room */
		switch (rand_int(4))
		{
			case 0: place_secret_door(wpos, y1b, xval); break;
			case 1: place_secret_door(wpos, y2b, xval); break;
			case 2: place_secret_door(wpos, yval, x1a); break;
			case 3: place_secret_door(wpos, yval, x2a); break;
		}

		/* Place a treasure in the vault */
		place_object(wpos, yval, xval, FALSE, FALSE, default_obj_theme);

		/* Let's guard the treasure well */
		vault_monsters(wpos, yval, xval, rand_int(2) + 3);

		/* Place the trap */
		if (magik(50)) place_trap(wpos, yval, xval, 0);

		/* Traps naturally */
		vault_traps(wpos, yval, xval, 4, 4, rand_int(3) + 2);

		break;


		/* Something else */
		case 3:

		/* Occasionally pinch the center shut */
		if (rand_int(3) == 0)
		{
			/* Pinch the east/west sides */
			for (y = y1b; y <= y2b; y++)
			{
				if (y == yval) continue;
				c_ptr = &zcave[y][x1a - 1];
				c_ptr->feat = FEAT_WALL_INNER;
				c_ptr = &zcave[y][x2a + 1];
				c_ptr->feat = FEAT_WALL_INNER;
			}

			/* Pinch the north/south sides */
			for (x = x1a; x <= x2a; x++)
			{
				if (x == xval) continue;
				c_ptr = &zcave[y1b - 1][x];
				c_ptr->feat = FEAT_WALL_INNER;
				c_ptr = &zcave[y2b + 1][x];
				c_ptr->feat = FEAT_WALL_INNER;
			}

			/* Sometimes shut using secret doors */
			if (rand_int(3) == 0)
			{
				place_secret_door(wpos, yval, x1a - 1);
				place_secret_door(wpos, yval, x2a + 1);
				place_secret_door(wpos, y1b - 1, xval);
				place_secret_door(wpos, y2b + 1, xval);
			}
		}

		/* Occasionally put a "plus" in the center */
		else if (rand_int(3) == 0)
		{
			c_ptr = &zcave[yval][xval];
			c_ptr->feat = FEAT_WALL_INNER;
			c_ptr = &zcave[y1b][xval];
			c_ptr->feat = FEAT_WALL_INNER;
			c_ptr = &zcave[y2b][xval];
			c_ptr->feat = FEAT_WALL_INNER;
			c_ptr = &zcave[yval][x1a];
			c_ptr->feat = FEAT_WALL_INNER;
			c_ptr = &zcave[yval][x2a];
			c_ptr->feat = FEAT_WALL_INNER;
		}

		/* Occasionally put a pillar in the center */
		else if (rand_int(3) == 0)
		{
			c_ptr = &zcave[yval][xval];
			c_ptr->feat = FEAT_WALL_INNER;
		}

		break;
	}
}


/*
 * Type 4 -- Large room with inner features
 *
 * Possible sub-types:
 *	1 - Just an inner room with one door
 *	2 - An inner room within an inner room
 *	3 - An inner room with pillar(s)
 *	4 - Inner room has a maze
 *	5 - A set of four inner rooms
 */
//static void build_type4(struct worldpos *wpos, int yval, int xval)
static void build_type4(struct worldpos *wpos, int by0, int bx0)
{
	int        y, x, y1, x1;
	int y2, x2, tmp, yval, xval;

	bool		light;
	cave_type *c_ptr;

	cave_type **zcave;
	if(!(zcave=getcave(wpos))) return;

	/* Try to allocate space for room.  If fails, exit */
	if (!room_alloc(wpos, 25, 11, FALSE, by0, bx0, &xval, &yval)) return;

	/* Choose lite or dark */
	light = (getlevel(wpos) <= randint(25));

	/* Large room */
	y1 = yval - 4;
	y2 = yval + 4;
	x1 = xval - 11;
	x2 = xval + 11;


	/* Place a full floor under the room */
	for (y = y1 - 1; y <= y2 + 1; y++)
	{
		for (x = x1 - 1; x <= x2 + 1; x++)
		{
			c_ptr = &zcave[y][x];
			c_ptr->feat = FEAT_FLOOR;
			c_ptr->info |= CAVE_ROOM;
			if (light) c_ptr->info |= CAVE_GLOW;
		}
	}

	/* Outer Walls */
	for (y = y1 - 1; y <= y2 + 1; y++)
	{
		c_ptr = &zcave[y][x1-1];
		c_ptr->feat = FEAT_WALL_OUTER;
		c_ptr = &zcave[y][x2+1];
		c_ptr->feat = FEAT_WALL_OUTER;
	}
	for (x = x1 - 1; x <= x2 + 1; x++)
	{
		c_ptr = &zcave[y1-1][x];
		c_ptr->feat = FEAT_WALL_OUTER;
		c_ptr = &zcave[y2+1][x];
		c_ptr->feat = FEAT_WALL_OUTER;
	}


	/* The inner room */
	y1 = y1 + 2;
	y2 = y2 - 2;
	x1 = x1 + 2;
	x2 = x2 - 2;

	/* The inner walls */
	for (y = y1 - 1; y <= y2 + 1; y++)
	{
		c_ptr = &zcave[y][x1-1];
		c_ptr->feat = FEAT_WALL_INNER;
		c_ptr = &zcave[y][x2+1];
		c_ptr->feat = FEAT_WALL_INNER;
	}
	for (x = x1 - 1; x <= x2 + 1; x++)
	{
		c_ptr = &zcave[y1-1][x];
		c_ptr->feat = FEAT_WALL_INNER;
		c_ptr = &zcave[y2+1][x];
		c_ptr->feat = FEAT_WALL_INNER;
	}


	/* Inner room variations */
	switch (randint(5))
	{
		/* Just an inner room with a monster */
		case 1:

		/* Place a secret door */
		switch (randint(4))
		{
			case 1: place_secret_door(wpos, y1 - 1, xval); break;
			case 2: place_secret_door(wpos, y2 + 1, xval); break;
			case 3: place_secret_door(wpos, yval, x1 - 1); break;
			case 4: place_secret_door(wpos, yval, x2 + 1); break;
		}

		/* Place a monster in the room */
		vault_monsters(wpos, yval, xval, 1);

		break;


		/* Treasure Vault (with a door) */
		case 2:

		/* Place a secret door */
		switch (randint(4))
		{
			case 1: place_secret_door(wpos, y1 - 1, xval); break;
			case 2: place_secret_door(wpos, y2 + 1, xval); break;
			case 3: place_secret_door(wpos, yval, x1 - 1); break;
			case 4: place_secret_door(wpos, yval, x2 + 1); break;
		}

		/* Place another inner room */
		for (y = yval - 1; y <= yval + 1; y++)
		{
			for (x = xval -  1; x <= xval + 1; x++)
			{
				if ((x == xval) && (y == yval)) continue;
				c_ptr = &zcave[y][x];
				c_ptr->feat = FEAT_WALL_INNER;
			}
		}

		/* Place a locked door on the inner room */
		switch (randint(4))
		{
			case 1: place_locked_door(wpos, yval - 1, xval); break;
			case 2: place_locked_door(wpos, yval + 1, xval); break;
			case 3: place_locked_door(wpos, yval, xval - 1); break;
			case 4: place_locked_door(wpos, yval, xval + 1); break;
		}

		/* Monsters to guard the "treasure" */
		vault_monsters(wpos, yval, xval, randint(3) + 2);

		/* Object (80%) */
		if (rand_int(100) < 80)
		{
			place_object(wpos, yval, xval, FALSE, FALSE, default_obj_theme);
		}

		/* Stairs (20%) */
		else
		{
			place_random_stairs(wpos, yval, xval);
		}

		/* Traps to protect the treasure */
		vault_traps(wpos, yval, xval, 4, 10, 2 + randint(3));

		break;


		/* Inner pillar(s). */
		case 3:

		/* Place a secret door */
		switch (randint(4))
		{
			case 1: place_secret_door(wpos, y1 - 1, xval); break;
			case 2: place_secret_door(wpos, y2 + 1, xval); break;
			case 3: place_secret_door(wpos, yval, x1 - 1); break;
			case 4: place_secret_door(wpos, yval, x2 + 1); break;
		}

		/* Large Inner Pillar */
		for (y = yval - 1; y <= yval + 1; y++)
		{
			for (x = xval - 1; x <= xval + 1; x++)
			{
				c_ptr = &zcave[y][x];
				c_ptr->feat = FEAT_WALL_INNER;
			}
		}

		/* Occasionally, two more Large Inner Pillars */
		if (rand_int(2) == 0)
		{
			tmp = randint(2);
			for (y = yval - 1; y <= yval + 1; y++)
			{
				for (x = xval - 5 - tmp; x <= xval - 3 - tmp; x++)
				{
					c_ptr = &zcave[y][x];
					c_ptr->feat = FEAT_WALL_INNER;
				}
				for (x = xval + 3 + tmp; x <= xval + 5 + tmp; x++)
				{
					c_ptr = &zcave[y][x];
					c_ptr->feat = FEAT_WALL_INNER;
				}
			}
		}

		/* Occasionally, some Inner rooms */
		if (rand_int(3) == 0)
		{
			/* Long horizontal walls */
			for (x = xval - 5; x <= xval + 5; x++)
			{
				c_ptr = &zcave[yval-1][x];
				c_ptr->feat = FEAT_WALL_INNER;
				c_ptr = &zcave[yval+1][x];
				c_ptr->feat = FEAT_WALL_INNER;
			}

			/* Close off the left/right edges */
#ifdef NEW_DUNGEON
			c_ptr = &zcave[yval][xval-5];
			c_ptr->feat = FEAT_WALL_INNER;
			c_ptr = &zcave[yval][xval+5];
			c_ptr->feat = FEAT_WALL_INNER;

			/* Secret doors (random top/bottom) */
			place_secret_door(wpos, yval - 3 + (randint(2) * 2), xval - 3);
			place_secret_door(wpos, yval - 3 + (randint(2) * 2), xval + 3);

			/* Monsters */
			vault_monsters(wpos, yval, xval - 2, randint(2));
			vault_monsters(wpos, yval, xval + 2, randint(2));

			/* Objects */
			if (rand_int(3) == 0) place_object(wpos, yval, xval - 2, FALSE, FALSE, default_obj_theme);
			if (rand_int(3) == 0) place_object(wpos, yval, xval + 2, FALSE, FALSE, default_obj_theme);
#else
			c_ptr = &cave[Depth][yval][xval-5];
			c_ptr->feat = FEAT_WALL_INNER;
			c_ptr = &cave[Depth][yval][xval+5];
			c_ptr->feat = FEAT_WALL_INNER;

			/* Secret doors (random top/bottom) */
			place_secret_door(Depth, yval - 3 + (randint(2) * 2), xval - 3);
			place_secret_door(Depth, yval - 3 + (randint(2) * 2), xval + 3);

			/* Monsters */
			vault_monsters(Depth, yval, xval - 2, randint(2));
			vault_monsters(Depth, yval, xval + 2, randint(2));

			/* Objects */
			if (rand_int(3) == 0) place_object(Depth, yval, xval - 2, FALSE, FALSE, default_obj_theme);
			if (rand_int(3) == 0) place_object(Depth, yval, xval + 2, FALSE, FALSE, default_obj_theme);
#endif
		}

		break;


		/* Maze inside. */
		case 4:

		/* Place a secret door */
		switch (randint(4))
		{
			case 1: place_secret_door(wpos, y1 - 1, xval); break;
			case 2: place_secret_door(wpos, y2 + 1, xval); break;
			case 3: place_secret_door(wpos, yval, x1 - 1); break;
			case 4: place_secret_door(wpos, yval, x2 + 1); break;
		}

		/* Maze (really a checkerboard) */
		for (y = y1; y <= y2; y++)
		{
			for (x = x1; x <= x2; x++)
			{
				if (0x1 & (x + y))
				{
					c_ptr = &zcave[y][x];
					c_ptr->feat = FEAT_WALL_INNER;
				}
			}
		}

		/* Monsters just love mazes. */
		vault_monsters(wpos, yval, xval - 5, randint(3));
		vault_monsters(wpos, yval, xval + 5, randint(3));

		/* Traps make them entertaining. */
		vault_traps(wpos, yval, xval - 3, 2, 8, randint(3));
		vault_traps(wpos, yval, xval + 3, 2, 8, randint(3));

		/* Mazes should have some treasure too. */
		vault_objects(wpos, yval, xval, 3);

		break;


		/* Four small rooms. */
		case 5:

		/* Inner "cross" */
		for (y = y1; y <= y2; y++)
		{
			c_ptr = &zcave[y][xval];
			c_ptr->feat = FEAT_WALL_INNER;
		}
		for (x = x1; x <= x2; x++)
		{
			c_ptr = &zcave[yval][x];
			c_ptr->feat = FEAT_WALL_INNER;
		}

		/* Doors into the rooms */
		if (rand_int(100) < 50)
		{
			int i = randint(10);
			place_secret_door(wpos, y1 - 1, xval - i);
			place_secret_door(wpos, y1 - 1, xval + i);
			place_secret_door(wpos, y2 + 1, xval - i);
			place_secret_door(wpos, y2 + 1, xval + i);
		}
		else
		{
			int i = randint(3);
			place_secret_door(wpos, yval + i, x1 - 1);
			place_secret_door(wpos, yval - i, x1 - 1);
			place_secret_door(wpos, yval + i, x2 + 1);
			place_secret_door(wpos, yval - i, x2 + 1);
		}

		/* Treasure, centered at the center of the cross */
		vault_objects(wpos, yval, xval, 2 + randint(2));

		/* Gotta have some monsters. */
		vault_monsters(wpos, yval + 1, xval - 4, randint(4));
		vault_monsters(wpos, yval + 1, xval + 4, randint(4));
		vault_monsters(wpos, yval - 1, xval - 4, randint(4));
		vault_monsters(wpos, yval - 1, xval + 4, randint(4));

		break;
	}
}


/*
 * The following functions are used to determine if the given monster
 * is appropriate for inclusion in a monster nest or monster pit or
 * the given type.
 *
 * None of the pits/nests are allowed to include "unique" monsters,
 * or monsters which can "multiply".
 *
 * Some of the pits/nests are asked to avoid monsters which can blink
 * away or which are invisible.  This is probably a hack.
 *
 * The old method made direct use of monster "names", which is bad.
 *
 * Note the use of Angband 2.7.9 monster race pictures in various places.
 */

/* Some new types of nests/pits are borrowed from ToME.		- Jir - */

/* Hack -- for clone pits */
static int template_race;

/*
 * Dungeon monster filter - not null
 */
bool dungeon_aux(int r_idx){
	monster_race *r_ptr = &r_info[r_idx];

	if (r_ptr->flags8 & RF8_DUNGEON)
		return TRUE;
	else
		return FALSE;
#if 0
//	if (dun->watery) return(TRUE);

	/* No aquatic life in the dungeon */
//	if (r_ptr->flags7 & RF7_AQUATIC) return(FALSE);
	return TRUE;
#endif	// 0
}

/*
 * Helper function for "monster nest (jelly)"
 */
static bool vault_aux_jelly(int r_idx)
{
	monster_race *r_ptr = &r_info[r_idx];

	/* Decline unique monsters */
	if (r_ptr->flags1 & RF1_UNIQUE) return (FALSE);

	/* Require icky thing, jelly, mold, or mushroom */
	if (!strchr("ijm,", r_ptr->d_char)) return (FALSE);

	/* Okay */
	return (TRUE);
}


/*
 * Helper function for "monster nest (animal)"
 */
static bool vault_aux_animal(int r_idx)
{
	monster_race *r_ptr = &r_info[r_idx];

	/* No aquatic life in the dungeon */
	if (r_ptr->flags7 & RF7_AQUATIC) return(FALSE);

	/* Decline unique monsters */
	if (r_ptr->flags1 & RF1_UNIQUE) return (FALSE);

	/* Require "animal" flag */
	if (!(r_ptr->flags3 & RF3_ANIMAL)) return (FALSE);

	/* Okay */
	return (TRUE);
}


/*
 * Helper function for "monster nest (undead)"
 */
static bool vault_aux_undead(int r_idx)
{
	monster_race *r_ptr = &r_info[r_idx];

	/* Decline unique monsters */
	if (r_ptr->flags1 & RF1_UNIQUE) return (FALSE);

	/* Require Undead */
	if (!(r_ptr->flags3 & RF3_UNDEAD)) return (FALSE);

	/* Okay */
	return (TRUE);
}


/*
 * Helper function for "monster nest (chapel)"
 */
static bool vault_aux_chapel(int r_idx)
{
	monster_race *r_ptr = &r_info[r_idx];

	/* Decline unique monsters */
	if (r_ptr->flags1 & (RF1_UNIQUE)) return (FALSE);

	/* Require "priest" or Angel */
	if (!((r_ptr->d_char == 'A') ||
		strstr((r_name + r_ptr->name),"riest")))
	{
		return (FALSE);
	}

	/* Okay */
	return (TRUE);
}


/*
 * Helper function for "monster nest (kennel)"
 */
static bool vault_aux_kennel(int r_idx)
{
	monster_race *r_ptr = &r_info[r_idx];

	/* Decline unique monsters */
	if (r_ptr->flags1 & (RF1_UNIQUE)) return (FALSE);

	/* Require a Zephyr Hound or a dog */
	return ((r_ptr->d_char == 'Z') || (r_ptr->d_char == 'C'));

}


/*
 * Helper function for "monster nest (treasure)"
 */
static bool vault_aux_treasure(int r_idx)
{
	monster_race *r_ptr = &r_info[r_idx];

	/* Decline unique monsters */
	if (r_ptr->flags1 & (RF1_UNIQUE)) return (FALSE);

	/* Hack -- allow mimics */
	if (r_ptr->flags9 & (RF9_MIMIC)) return (TRUE);

	/* Require Object form */
	if (!((r_ptr->d_char == '!') || (r_ptr->d_char == '|') ||
		(r_ptr->d_char == '$') || (r_ptr->d_char == '?') ||
		(r_ptr->d_char == '=')))
	{
		return (FALSE);
	}

	/* Okay */
	return (TRUE);
}


/*
 * Helper function for "monster nest (clone)"
 */
static bool vault_aux_clone(int r_idx)
{
	return (r_idx == template_race);
}


/*
 * Helper function for "monster nest (symbol clone)"
 */
static bool vault_aux_symbol(int r_idx)
{
	return ((r_info[r_idx].d_char == (r_info[template_race].d_char))
		&& !(r_info[r_idx].flags1 & RF1_UNIQUE));
}



/*
 * Helper function for "monster pit (orc)"
 */
static bool vault_aux_orc(int r_idx)
{
	monster_race *r_ptr = &r_info[r_idx];

	/* Decline unique monsters */
	if (r_ptr->flags1 & RF1_UNIQUE) return (FALSE);

	/* Hack -- Require "o" monsters */
	if (!strchr("o", r_ptr->d_char)) return (FALSE);

	/* Okay */
	return (TRUE);
}


/*
 * Helper function for "monster pit (orc and ogre)"
 */
static bool vault_aux_orc_ogre(int r_idx)
{
	monster_race *r_ptr = &r_info[r_idx];

	/* Decline unique monsters */
	if (r_ptr->flags1 & RF1_UNIQUE) return (FALSE);

	/* Hack -- Require "o" monsters */
	if (!strchr("oO", r_ptr->d_char)) return (FALSE);

	/* Okay */
	return (TRUE);
}


/*
 * Helper function for "monster pit (troll)"
 */
static bool vault_aux_troll(int r_idx)
{
	monster_race *r_ptr = &r_info[r_idx];

	/* Decline unique monsters */
	if (r_ptr->flags1 & RF1_UNIQUE) return (FALSE);

	/* Hack -- Require "T" monsters */
	if (!strchr("T", r_ptr->d_char)) return (FALSE);

	/* Okay */
	return (TRUE);
}


/*
 * Helper function for "monster pit (mankind)"
 */
static bool vault_aux_man(int r_idx)
{
	monster_race *r_ptr = &r_info[r_idx];

	/* Decline unique monsters */
	if (r_ptr->flags1 & RF1_UNIQUE) return (FALSE);

	/* Hack -- Require "p" or "h" monsters */
	if (!strchr("ph", r_ptr->d_char)) return (FALSE);

	/* Okay */
	return (TRUE);
}


/*
 * Helper function for "monster pit (giant)"
 */
static bool vault_aux_giant(int r_idx)
{
	monster_race *r_ptr = &r_info[r_idx];

	/* Decline unique monsters */
	if (r_ptr->flags1 & RF1_UNIQUE) return (FALSE);

	/* Hack -- Require "P" monsters */
	if (!strchr("P", r_ptr->d_char)) return (FALSE);

	/* Okay */
	return (TRUE);
}


/*
 * Hack -- breath type for "vault_aux_dragon()"
 */
static u32b vault_aux_dragon_mask4;


/*
 * Helper function for "monster pit (dragon)"
 */
static bool vault_aux_dragon(int r_idx)
{
	monster_race *r_ptr = &r_info[r_idx];

	/* Decline unique monsters */
	if (r_ptr->flags1 & RF1_UNIQUE) return (FALSE);

	/* Hack -- Require "d" or "D" monsters */
	if (!strchr("Dd", r_ptr->d_char)) return (FALSE);

	/* Hack -- Allow 'all stars' type */
	if (!vault_aux_dragon_mask4) return (TRUE);

	/* Hack -- Require correct "breath attack" */
	if (r_ptr->flags4 != vault_aux_dragon_mask4) return (FALSE);

	/* Okay */
	return (TRUE);
}


/*
 * Helper function for "monster pit (demon)"
 */
static bool vault_aux_demon(int r_idx)
{
	monster_race *r_ptr = &r_info[r_idx];

	/* Decline unique monsters */
	if (r_ptr->flags1 & RF1_UNIQUE) return (FALSE);

	/* Hack -- Require "U" monsters */
	if (!strchr("U", r_ptr->d_char)) return (FALSE);

	/* Okay */
	return (TRUE);
}



/*
 * Type 5 -- Monster nests
 *
 * A monster nest is a "big" room, with an "inner" room, containing
 * a "collection" of monsters of a given type strewn about the room.
 *
 * The monsters are chosen from a set of 64 randomly selected monster
 * races, to allow the nest creation to fail instead of having "holes".
 *
 * Note the use of the "get_mon_num_prep()" function, and the special
 * "get_mon_num_hook()" restriction function, to prepare the "monster
 * allocation table" in such a way as to optimize the selection of
 * "appropriate" non-unique monsters for the nest.
 *
 * Currently, a monster nest is one of
 *   a nest of "jelly" monsters   (Dungeon level 5 and deeper)
 *   a nest of "animal" monsters  (Dungeon level 30 and deeper)
 *   a nest of "undead" monsters  (Dungeon level 50 and deeper)
 *
 * Note that the "get_mon_num()" function may (rarely) fail, in which
 * case the nest will be empty, and will not affect the level rating.
 *
 * Note that "monster nests" will never contain "unique" monsters.
 */
//static void build_type5(struct worldpos *wpos, int yval, int xval)
static void build_type5(struct worldpos *wpos, int by0, int bx0)
{
	int y, x, y1, x1, y2, x2, xval, yval;

	int			tmp, i, dun_level;

	s16b		what[64];

	cave_type		*c_ptr;

	cptr		name;

	bool		empty = FALSE;
	cave_type **zcave;
	if(!(zcave=getcave(wpos))) return;

	dun_level = getlevel(wpos);

	/* Try to allocate space for room.  If fails, exit */
	if (!room_alloc(wpos, 25, 11, TRUE, by0, bx0, &xval, &yval)) return;

	/* Large room */
	y1 = yval - 4;
	y2 = yval + 4;
	x1 = xval - 11;
	x2 = xval + 11;


	/* Place the floor area */
	for (y = y1 - 1; y <= y2 + 1; y++)
	{
		for (x = x1 - 1; x <= x2 + 1; x++)
		{
			c_ptr = &zcave[y][x];
			c_ptr->feat = FEAT_FLOOR;
			c_ptr->info |= CAVE_ROOM;
		}
	}

	/* Place the outer walls */
	for (y = y1 - 1; y <= y2 + 1; y++)
	{
		c_ptr = &zcave[y][x1-1];
		c_ptr->feat = FEAT_WALL_OUTER;
		c_ptr = &zcave[y][x2+1];
		c_ptr->feat = FEAT_WALL_OUTER;
	}
	for (x = x1 - 1; x <= x2 + 1; x++)
	{
		c_ptr = &zcave[y1-1][x];
		c_ptr->feat = FEAT_WALL_OUTER;
		c_ptr = &zcave[y2+1][x];
		c_ptr->feat = FEAT_WALL_OUTER;
	}


	/* Advance to the center room */
	y1 = y1 + 2;
	y2 = y2 - 2;
	x1 = x1 + 2;
	x2 = x2 - 2;

	/* The inner walls */
	for (y = y1 - 1; y <= y2 + 1; y++)
	{
		c_ptr = &zcave[y][x1-1];
		c_ptr->feat = FEAT_WALL_INNER;
		c_ptr = &zcave[y][x2+1];
		c_ptr->feat = FEAT_WALL_INNER;
	}
	for (x = x1 - 1; x <= x2 + 1; x++)
	{
		c_ptr = &zcave[y1-1][x];
		c_ptr->feat = FEAT_WALL_INNER;
		c_ptr = &zcave[y2+1][x];
		c_ptr->feat = FEAT_WALL_INNER;
	}


	/* Place a secret door */
	switch (randint(4))
	{
		case 1: place_secret_door(wpos, y1 - 1, xval); break;
		case 2: place_secret_door(wpos, y2 + 1, xval); break;
		case 3: place_secret_door(wpos, yval, x1 - 1); break;
		case 4: place_secret_door(wpos, yval, x2 + 1); break;
	}


	/* Hack -- Choose a nest type */
	tmp = randint(dun_level);

	if ((tmp < 25) && (rand_int(5) != 0))	// rand_int(2)
	{
		while (1)
		{
			template_race = randint(MAX_R_IDX - 2);

			/* Reject uniques */
			if (r_info[template_race].flags1 & RF1_UNIQUE) continue;

			/* Reject OoD monsters in a loose fashion */
		    if (((r_info[template_race].level) + randint(5)) >
			    (dun_level + randint(5))) continue;

			/* Don't like 'break's like this, but this cannot be made better */
			break;
		}

		if ((dun_level >= (25 + randint(15))) && (rand_int(2) != 0))
		{
			name = "symbol clone";
			get_mon_num_hook = vault_aux_symbol;
		}
		else
		{
			name = "clone";
			get_mon_num_hook = vault_aux_clone;
		}
	}
	else if (tmp < 35)	// 25
		/* Monster nest (jelly) */
	{
		/* Describe */
		name = "jelly";

		/* Restrict to jelly */
		get_mon_num_hook = vault_aux_jelly;
	}

	else if (tmp < 45)	// 50 ... we need mimics
	{
		name = "treasure";
		get_mon_num_hook = vault_aux_treasure;
	}

	/* Monster nest (animal) */
	else if (tmp < 65)
	{
		if (rand_int(3) == 0)
		{
			name = "kennel";
			get_mon_num_hook = vault_aux_kennel;
		}
		else
		{
			/* Describe */
			name = "animal";

			/* Restrict to animal */
			get_mon_num_hook = vault_aux_animal;
		}
	}

	/* Monster nest (undead) */
	else
	{
		if (rand_int(3) == 0)
		{
			name = "chapel";
			get_mon_num_hook = vault_aux_chapel;
		}
		else
		{
			/* Describe */
			name = "undead";

			/* Restrict to undead */
			get_mon_num_hook = vault_aux_undead;
		}
	}

#if 0
	/* Monster nest (jelly) */
	if (tmp < 30)
	{
		/* Describe */
		name = "jelly";

		/* Restrict to jelly */
		get_mon_num_hook = vault_aux_jelly;
	}

	/* Monster nest (animal) */
	else if (tmp < 50)
	{
		/* Describe */
		name = "animal";

		/* Restrict to animal */
		get_mon_num_hook = vault_aux_animal;
	}

	/* Monster nest (undead) */
	else
	{
		/* Describe */
		name = "undead";

		/* Restrict to undead */
		get_mon_num_hook = vault_aux_undead;
	}
#endif	// 0

	/* Prepare allocation table */
	get_mon_num_prep();


	/* Pick some monster types */
	for (i = 0; i < 64; i++)
	{
		/* Get a (hard) monster type */
		what[i] = get_mon_num(getlevel(wpos) + 10);

		/* Notice failure */
		if (!what[i]) empty = TRUE;
	}


	/* Remove restriction */
	get_mon_num_hook = dungeon_aux;

	/* Prepare allocation table */
	get_mon_num_prep();


	/* Oops */
	if (empty) return;

	/* Place some monsters */
	for (y = yval - 2; y <= yval + 2; y++)
	{
		for (x = xval - 9; x <= xval + 9; x++)
		{
			int r_idx = what[rand_int(64)];

			/* Place that "random" monster (no groups) */
			(void)place_monster_aux(wpos, y, x, r_idx, FALSE, FALSE, FALSE);
		}
	}
}



/*
 * Type 6 -- Monster pits
 *
 * A monster pit is a "big" room, with an "inner" room, containing
 * a "collection" of monsters of a given type organized in the room.
 *
 * Monster types in the pit
 *   orc pit	(Dungeon Level 5 and deeper)
 *   troll pit	(Dungeon Level 20 and deeper)
 *   giant pit	(Dungeon Level 40 and deeper)
 *   dragon pit	(Dungeon Level 60 and deeper)
 *   demon pit	(Dungeon Level 80 and deeper)
 *
 * The inside room in a monster pit appears as shown below, where the
 * actual monsters in each location depend on the type of the pit
 *
 *   #####################
 *   #0000000000000000000#
 *   #0112233455543322110#
 *   #0112233467643322110#
 *   #0112233455543322110#
 *   #0000000000000000000#
 *   #####################
 *
 * Note that the monsters in the pit are now chosen by using "get_mon_num()"
 * to request 16 "appropriate" monsters, sorting them by level, and using
 * the "even" entries in this sorted list for the contents of the pit.
 *
 * Hack -- all of the "dragons" in a "dragon" pit must be the same "color",
 * which is handled by requiring a specific "breath" attack for all of the
 * dragons.  This may include "multi-hued" breath.  Note that "wyrms" may
 * be present in many of the dragon pits, if they have the proper breath.
 *
 * Note the use of the "get_mon_num_prep()" function, and the special
 * "get_mon_num_hook()" restriction function, to prepare the "monster
 * allocation table" in such a way as to optimize the selection of
 * "appropriate" non-unique monsters for the pit.
 *
 * Note that the "get_mon_num()" function may (rarely) fail, in which case
 * the pit will be empty, and will not effect the level rating.
 *
 * Note that "monster pits" will never contain "unique" monsters.
 */
#define BUILD_6_MONSTER_TABLE	32
//static void build_type6(struct worldpos *wpos, int yval, int xval)
static void build_type6(struct worldpos *wpos, int by0, int bx0)
{
	int			tmp, what[BUILD_6_MONSTER_TABLE];

	int			i, j, y, x, y1, x1, y2, x2, dun_level, xval, yval;

	bool		empty = FALSE, aqua = magik(dun->watery? 50 : 10);

	cave_type		*c_ptr;

	cptr		name;
	cave_type **zcave;
	if(!(zcave=getcave(wpos))) return;

	/* Try to allocate space for room.  If fails, exit */
	if (!room_alloc(wpos, 25, 11, TRUE, by0, bx0, &xval, &yval)) return;

	dun_level = getlevel(wpos);

	/* Large room */
	y1 = yval - 4;
	y2 = yval + 4;
	x1 = xval - 11;
	x2 = xval + 11;


	/* Place the floor area */
	for (y = y1 - 1; y <= y2 + 1; y++)
	{
		for (x = x1 - 1; x <= x2 + 1; x++)
		{
			c_ptr = &zcave[y][x];
			c_ptr->feat = aqua ? FEAT_DEEP_WATER : FEAT_FLOOR;
			c_ptr->info |= CAVE_ROOM;
		}
	}

	/* Place the outer walls */
	for (y = y1 - 1; y <= y2 + 1; y++)
	{
		c_ptr = &zcave[y][x1-1];
		c_ptr->feat = FEAT_WALL_OUTER;
		c_ptr = &zcave[y][x2+1];
		c_ptr->feat = FEAT_WALL_OUTER;
	}
	for (x = x1 - 1; x <= x2 + 1; x++)
	{
		c_ptr = &zcave[y1-1][x];
		c_ptr->feat = FEAT_WALL_OUTER;
		c_ptr = &zcave[y2+1][x];
		c_ptr->feat = FEAT_WALL_OUTER;
	}


	/* Advance to the center room */
	y1 = y1 + 2;
	y2 = y2 - 2;
	x1 = x1 + 2;
	x2 = x2 - 2;

	/* The inner walls */
	for (y = y1 - 1; y <= y2 + 1; y++)
	{
		c_ptr = &zcave[y][x1-1];
		c_ptr->feat = FEAT_WALL_INNER;
		c_ptr = &zcave[y][x2+1];
		c_ptr->feat = FEAT_WALL_INNER;
	}
	for (x = x1 - 1; x <= x2 + 1; x++)
	{
		c_ptr = &zcave[y1-1][x];
		c_ptr->feat = FEAT_WALL_INNER;
		c_ptr = &zcave[y2+1][x];
		c_ptr->feat = FEAT_WALL_INNER;
	}


	/* Place a secret door */
	switch (randint(4))
	{
		case 1: place_secret_door(wpos, y1 - 1, xval); break;
		case 2: place_secret_door(wpos, y2 + 1, xval); break;
		case 3: place_secret_door(wpos, yval, x1 - 1); break;
		case 4: place_secret_door(wpos, yval, x2 + 1); break;
	}


	/* Choose a pit type */
	tmp = randint(dun_level);

	/* Watery pit */
	if (aqua)
	{
		/* Message */
		name = "aquatic";

		/* Restrict monster selection */
		get_mon_num_hook = vault_aux_aquatic;
	}

	/* Orc pit */
	else if (tmp < 15)	// 20
	{
		if (dun_level > 30 && magik(50))
		{
			/* Message */
			name = "orc and ogre";

			/* Restrict monster selection */
			get_mon_num_hook = vault_aux_orc_ogre;
		}
		else
		{
			/* Message */
			name = "orc";

			/* Restrict monster selection */
			get_mon_num_hook = vault_aux_orc;
		}
	}

	/* Troll pit */
	else if (tmp < 30)	// 40
	{
		/* Message */
		name = "troll";

		/* Restrict monster selection */
		get_mon_num_hook = vault_aux_troll;
	}

	/* Man pit */
	else if (tmp < 40)
	{
		/* Message */
		name = "mankind";

		/* Restrict monster selection */
		get_mon_num_hook = vault_aux_man;
	}

	/* Giant pit */
	else if (tmp < 55)
	{
		/* Message */
		name = "giant";

		/* Restrict monster selection */
		get_mon_num_hook = vault_aux_giant;
	}

	else if (tmp < 70)
	{
		if (randint(4)!=1)
		{
			/* Message */
			name = "ordered clones";

			do { template_race = randint(MAX_R_IDX - 2); }
			while ((r_info[template_race].flags1 & RF1_UNIQUE)
			       || (((r_info[template_race].level) + randint(5)) >
				   (dun_level + randint(5))));

			/* Restrict selection */
			get_mon_num_hook = vault_aux_symbol;
		}
		else
		{

			name = "ordered chapel";
			get_mon_num_hook = vault_aux_chapel;
		}

	}

	/* Dragon pit */
	else if (tmp < 80)
	{
		/* Pick dragon type */
		switch (rand_int(7))
		{
			/* Black */
			case 0:
			{
				/* Message */
				name = "acid dragon";

				/* Restrict dragon breath type */
				vault_aux_dragon_mask4 = RF4_BR_ACID;

				/* Done */
				break;
			}

			/* Blue */
			case 1:
			{
				/* Message */
				name = "electric dragon";

				/* Restrict dragon breath type */
				vault_aux_dragon_mask4 = RF4_BR_ELEC;

				/* Done */
				break;
			}

			/* Red */
			case 2:
			{
				/* Message */
				name = "fire dragon";

				/* Restrict dragon breath type */
				vault_aux_dragon_mask4 = RF4_BR_FIRE;

				/* Done */
				break;
			}

			/* White */
			case 3:
			{
				/* Message */
				name = "cold dragon";

				/* Restrict dragon breath type */
				vault_aux_dragon_mask4 = RF4_BR_COLD;

				/* Done */
				break;
			}

			/* Green */
			case 4:
			{
				/* Message */
				name = "poison dragon";

				/* Restrict dragon breath type */
				vault_aux_dragon_mask4 = RF4_BR_POIS;

				/* Done */
				break;
			}

			/* All stars */
			case 5:
			{
				/* Message */
				name = "dragon";

				/* Restrict dragon breath type */
				vault_aux_dragon_mask4 = 0L;

				/* Done */
				break;
			}

			/* Multi-hued */
			default:
			{
				/* Message */
				name = "multi-hued dragon";

				/* Restrict dragon breath type */
				vault_aux_dragon_mask4 = (RF4_BR_ACID | RF4_BR_ELEC |
				                          RF4_BR_FIRE | RF4_BR_COLD |
				                          RF4_BR_POIS);

				/* Done */
				break;
			}

		}

		/* Restrict monster selection */
		get_mon_num_hook = vault_aux_dragon;
	}

	/* Demon pit */
	else
	{
		/* Message */
		name = "demon";

		/* Restrict monster selection */
		get_mon_num_hook = vault_aux_demon;
	}

	/* Prepare allocation table */
	get_mon_num_prep();


	/* Pick some monster types */
	for (i = 0; i < BUILD_6_MONSTER_TABLE; i++)
	{
		/* Get a (hard) monster type */
		what[i] = get_mon_num(getlevel(wpos) + 10);

		/* Notice failure */
		if (!what[i]) empty = TRUE;
	}


	/* Remove restriction */
	get_mon_num_hook = dungeon_aux;

	/* Prepare allocation table */
	get_mon_num_prep();


	/* Oops */
	if (empty) return;


	/* XXX XXX XXX */
	/* Sort the entries */
	for (i = 0; i < BUILD_6_MONSTER_TABLE - 1; i++)
	{
		/* Sort the entries */
		for (j = 0; j < BUILD_6_MONSTER_TABLE - 1; j++)
		{
			int i1 = j;
			int i2 = j + 1;

			int p1 = r_info[what[i1]].level;
			int p2 = r_info[what[i2]].level;

			/* Bubble */
			if (p1 > p2)
			{
				int tmp = what[i1];
				what[i1] = what[i2];
				what[i2] = tmp;
			}
		}
	}

	/* Select the entries */
	for (i = 0; i < 8; i++)
	{
		/* Every other entry */
		what[i] = what[i * 2];
	}

	/* Top and bottom rows */
	for (x = xval - 9; x <= xval + 9; x++)
	{
		place_monster_aux(wpos, yval - 2, x, what[0], FALSE, FALSE, FALSE);
		place_monster_aux(wpos, yval + 2, x, what[0], FALSE, FALSE, FALSE);
	}

	/* Middle columns */
	for (y = yval - 1; y <= yval + 1; y++)
	{
#ifdef NEW_DUNGEON
		place_monster_aux(wpos, y, xval - 9, what[0], FALSE, FALSE, FALSE);
		place_monster_aux(wpos, y, xval + 9, what[0], FALSE, FALSE, FALSE);

		place_monster_aux(wpos, y, xval - 8, what[1], FALSE, FALSE, FALSE);
		place_monster_aux(wpos, y, xval + 8, what[1], FALSE, FALSE, FALSE);

		place_monster_aux(wpos, y, xval - 7, what[1], FALSE, FALSE, FALSE);
		place_monster_aux(wpos, y, xval + 7, what[1], FALSE, FALSE, FALSE);

		place_monster_aux(wpos, y, xval - 6, what[2], FALSE, FALSE, FALSE);
		place_monster_aux(wpos, y, xval + 6, what[2], FALSE, FALSE, FALSE);

		place_monster_aux(wpos, y, xval - 5, what[2], FALSE, FALSE, FALSE);
		place_monster_aux(wpos, y, xval + 5, what[2], FALSE, FALSE, FALSE);

		place_monster_aux(wpos, y, xval - 4, what[3], FALSE, FALSE, FALSE);
		place_monster_aux(wpos, y, xval + 4, what[3], FALSE, FALSE, FALSE);

		place_monster_aux(wpos, y, xval - 3, what[3], FALSE, FALSE, FALSE);
		place_monster_aux(wpos, y, xval + 3, what[3], FALSE, FALSE, FALSE);

		place_monster_aux(wpos, y, xval - 2, what[4], FALSE, FALSE, FALSE);
		place_monster_aux(wpos, y, xval + 2, what[4], FALSE, FALSE, FALSE);
	}

	/* Above/Below the center monster */
	for (x = xval - 1; x <= xval + 1; x++)
	{
		place_monster_aux(wpos, yval + 1, x, what[5], FALSE, FALSE, FALSE);
		place_monster_aux(wpos, yval - 1, x, what[5], FALSE, FALSE, FALSE);
	}

	/* Next to the center monster */
	place_monster_aux(wpos, yval, xval + 1, what[6], FALSE, FALSE, FALSE);
	place_monster_aux(wpos, yval, xval - 1, what[6], FALSE, FALSE, FALSE);

	/* Center monster */
	place_monster_aux(wpos, yval, xval, what[7], FALSE, FALSE, FALSE);
#else
		place_monster_aux(Depth, y, xval - 9, what[0], FALSE, FALSE, FALSE);
		place_monster_aux(Depth, y, xval + 9, what[0], FALSE, FALSE, FALSE);

		place_monster_aux(Depth, y, xval - 8, what[1], FALSE, FALSE, FALSE);
		place_monster_aux(Depth, y, xval + 8, what[1], FALSE, FALSE, FALSE);

		place_monster_aux(Depth, y, xval - 7, what[1], FALSE, FALSE, FALSE);
		place_monster_aux(Depth, y, xval + 7, what[1], FALSE, FALSE, FALSE);

		place_monster_aux(Depth, y, xval - 6, what[2], FALSE, FALSE, FALSE);
		place_monster_aux(Depth, y, xval + 6, what[2], FALSE, FALSE, FALSE);

		place_monster_aux(Depth, y, xval - 5, what[2], FALSE, FALSE, FALSE);
		place_monster_aux(Depth, y, xval + 5, what[2], FALSE, FALSE, FALSE);

		place_monster_aux(Depth, y, xval - 4, what[3], FALSE, FALSE, FALSE);
		place_monster_aux(Depth, y, xval + 4, what[3], FALSE, FALSE, FALSE);

		place_monster_aux(Depth, y, xval - 3, what[3], FALSE, FALSE, FALSE);
		place_monster_aux(Depth, y, xval + 3, what[3], FALSE, FALSE, FALSE);

		place_monster_aux(Depth, y, xval - 2, what[4], FALSE, FALSE, FALSE);
		place_monster_aux(Depth, y, xval + 2, what[4], FALSE, FALSE, FALSE);
	}

	/* Above/Below the center monster */
	for (x = xval - 1; x <= xval + 1; x++)
	{
		place_monster_aux(Depth, yval + 1, x, what[5], FALSE, FALSE, FALSE);
		place_monster_aux(Depth, yval - 1, x, what[5], FALSE, FALSE, FALSE);
	}

	/* Next to the center monster */
	place_monster_aux(Depth, yval, xval + 1, what[6], FALSE, FALSE, FALSE);
	place_monster_aux(Depth, yval, xval - 1, what[6], FALSE, FALSE, FALSE);

	/* Center monster */
	place_monster_aux(Depth, yval, xval, what[7], FALSE, FALSE, FALSE);
#endif
}



/*
 * Hack -- fill in "vault" rooms
 */
//void build_vault(struct worldpos *wpos, int yval, int xval, int ymax, int xmax, cptr data)
void build_vault(struct worldpos *wpos, int yval, int xval, vault_type *v_ptr)
{
	int bwy[8], bwx[8], i;
	int dx, dy, x, y, cx, cy, lev = getlevel(wpos);

	cptr t;

	cave_type *c_ptr;
	cave_type **zcave;
	c_special *cs_ptr;
	dun_level *l_ptr = getfloor(wpos);
	bool hives = FALSE, mirrorlr = FALSE, mirrorud = FALSE,
		rotate = FALSE, force = FALSE;
	int ymax = v_ptr->hgt, xmax = v_ptr->wid;
	char *data = v_text + v_ptr->text;

	if(!(zcave=getcave(wpos))) return;

	if (v_ptr->flags1 & VF1_NO_PENETR) dun->no_penetr = TRUE;
	if (v_ptr->flags1 & VF1_HIVES) hives = TRUE;

	if (!hives)	/* Hack -- avoid ugly results */
	{
		if (!(v_ptr->flags1 & VF1_NO_MIRROR))
		{
			if (magik(30)) mirrorlr = TRUE;
			if (magik(30)) mirrorud = TRUE;
		}
		if (!(v_ptr->flags1 & VF1_NO_ROTATE) && magik(30)) rotate = TRUE;
	}

	cx = xval - ((rotate?ymax:xmax) / 2) * (mirrorlr?-1:1);
	cy = yval - ((rotate?xmax:ymax) / 2) * (mirrorud?-1:1);

	/* At least 1/4 should be genetated */
	if (!in_bounds4(l_ptr, cy, cx))
		return;

	/* Check for flags */
	if (v_ptr->flags1 & VF1_FORCE_FLAGS) force = TRUE;
	if (v_ptr->flags1 & VF1_NO_TELEPORT && (magik(VAULT_FLAG_CHANCE) || force))
		l_ptr->flags1 |= LF1_NO_TELEPORT;
	if (v_ptr->flags1 & VF1_NO_GENO && (magik(VAULT_FLAG_CHANCE) || force))
		l_ptr->flags1 |= LF1_NO_GENO;
	if (v_ptr->flags1 & VF1_NOMAP && (magik(VAULT_FLAG_CHANCE) || force))
		l_ptr->flags1 |= LF1_NOMAP;
	if (v_ptr->flags1 & VF1_NO_MAGIC_MAP && (magik(VAULT_FLAG_CHANCE) || force))
		l_ptr->flags1 |= LF1_NO_MAGIC_MAP;
	if (v_ptr->flags1 & VF1_NO_DESTROY && (magik(VAULT_FLAG_CHANCE) || force))
		l_ptr->flags1 |= LF1_NO_DESTROY;
	if (v_ptr->flags1 & VF1_NO_MAGIC && (magik(VAULT_FLAG_CHANCE) || force)
			&& lev < 100)
		l_ptr->flags1 |= LF1_NO_MAGIC;

	/* Clean the between gates arrays */
	for(i = 0; i < 8; i++)
	{
		bwy[i] = bwx[i] = 9999;
	}

	/* Place dungeon features and objects */
	for (t = data, dy = 0; dy < ymax; dy++)
	{
		for (dx = 0; dx < xmax; dx++, t++)
		{
			/* Extract the location */
/*			x = xval - (xmax / 2) + dx;
			y = yval - (ymax / 2) + dy;	*/
			x = cx + (rotate?dy:dx) * (mirrorlr?-1:1);
			y = cy + (rotate?dx:dy) * (mirrorud?-1:1);

			/* FIXME - find a better solution */
			/* Is this any better? */
			if(!in_bounds4(l_ptr,y,x))
				continue;
			
			/* Hack -- skip "non-grids" */
			if (*t == ' ') continue;

			/* Access the grid */
			c_ptr = &zcave[y][x];

			/* Lay down a floor */
			c_ptr->feat = FEAT_FLOOR;

			/* Part of a vault */
			c_ptr->info |= (CAVE_ROOM | CAVE_ICKY);

			/* Analyze the grid */
			switch (*t)
			{
				/* Granite wall (outer) */
				case '%':
				c_ptr->feat = FEAT_WALL_OUTER;
				break;

				/* Granite wall (inner) */
				case '#':
				c_ptr->feat = FEAT_WALL_INNER;
				break;

				/* Permanent wall (inner) */
				case 'X':
				c_ptr->feat = FEAT_PERM_INNER;
				break;

				/* Treasure/trap */
				case '*':
				if (rand_int(100) < 75)
				{
					place_object(wpos, y, x, FALSE, FALSE, default_obj_theme);
				}
//				else	// now cumulative :)
//				if (rand_int(100) < 25)
				if (rand_int(100) < 40)
				{
					place_trap(wpos, y, x, 0);
				}
				break;

				/* Secret doors */
				case '+':
				place_secret_door(wpos, y, x);
				if (magik(20))
				{
					place_trap(wpos, y, x, 0);
				}
				break;

				/* Trap */
				case '^':
				place_trap(wpos, y, x, 0);
				break;

				/* Monster */
				case '&':
				monster_level = lev + 5;
				place_monster(wpos, y, x, TRUE, TRUE);
				monster_level = lev;
				break;

				/* Meaner monster */
				case '@':
				monster_level = lev + 11;
				place_monster(wpos, y, x, TRUE, TRUE);
				monster_level = lev;
				break;

				/* Meaner monster, plus treasure */
				case '9':
				monster_level = lev + 9;
				place_monster(wpos, y, x, TRUE, TRUE);
				monster_level = lev;
				object_level = lev + 7;
				place_object(wpos, y, x, TRUE, FALSE, default_obj_theme);
				object_level = lev;
				if (magik(40)) place_trap(wpos, y, x, 0);
				break;

				/* Nasty monster and treasure */
				case '8':
				monster_level = lev + 40;
				place_monster(wpos, y, x, TRUE, TRUE);
				monster_level = lev;
				object_level = lev + 20;
				place_object(wpos, y, x, TRUE, TRUE, default_obj_theme);
				object_level = lev;
				if (magik(80)) place_trap(wpos, y, x, 0);
				break;

				/* Monster and/or object */
				case ',':
				if (magik(50))
				{
					monster_level = lev + 3;
					place_monster(wpos, y, x, TRUE, TRUE);
					monster_level = lev;
				}
				if (magik(50))
				{
					object_level = lev + 7;
					place_object(wpos, y, x, FALSE, FALSE, default_obj_theme);
					object_level = lev;
				}
				if (magik(50)) place_trap(wpos, y, x, 0);
				break;

				/* Between gates */
				case '0':
				case '1':
				case '2':
				case '3':
				case '4':
				case '5':
				case '6':
				case '7':
				{
					/* XXX not sure what will happen if the cave already has
					 * another between gate */
					/* Not found before */
					if(bwy[*t - '0'] == 9999)
					{
						if (!(cs_ptr=AddCS(c_ptr, CS_BETWEEN)))
						{
#if DEBUG_LEVEL > 0
							s_printf("oops, vault between gates generation failed(1st: %s)!!\n", wpos_format(0, wpos));
#endif // DEBUG_LEVEL
							break;
						}
						cave_set_feat(wpos, y, x, FEAT_BETWEEN);
						bwy[*t - '0'] = y;
						bwx[*t - '0'] = x;
					}
					/* The second time */
					else
					{
						if (!(cs_ptr=AddCS(c_ptr, CS_BETWEEN)))
						{
#if DEBUG_LEVEL > 0
							s_printf("oops, vault between gates generation failed(2nd: %s)!!\n", wpos_format(0, wpos));
#endif // DEBUG_LEVEL
							break;
						}
						cave_set_feat(wpos, y, x, FEAT_BETWEEN);
						cs_ptr->sc.between.fy = bwy[*t - '0'];
						cs_ptr->sc.between.fx = bwx[*t - '0'];

						cs_ptr=GetCS(&zcave[bwy[*t - '0']][bwx[*t - '0']], CS_BETWEEN);
						cs_ptr->sc.between.fy = y;
						cs_ptr->sc.between.fx = x;
#if 0
						c_ptr->special = bwx[*t - '0'] + (bwy[*t - '0'] << 8);
						cave[bwy[*t - '0']][bwx[*t - '0']].special = x + (y << 8);
#endif	// 0
					}
					break;
				}
			}
		}
	}


	/* Reproduce itself */
	/* TODO: make a better routine! */
	if (hives)
	{
		if (magik(HIVE_CHANCE(lev)) && !magik(ymax))
			build_vault(wpos, yval + ymax, xval, v_ptr);
		if (magik(HIVE_CHANCE(lev)) && !magik(xmax))
			build_vault(wpos, yval, xval + xmax, v_ptr);

//		build_vault(wpos, yval - ymax, xval, &v_ptr);
//		build_vault(wpos, yval, xval - xmax, &v_ptr);
	}
}



/*
 * Type 7 -- simple vaults (see "v_info.txt")
 */
//static void build_type7(struct worldpos *wpos, int yval, int xval)
static void build_type7(struct worldpos *wpos, int by0, int bx0)
{
	vault_type	*v_ptr;
	int xval, yval;
	cave_type **zcave;
	if(!(zcave=getcave(wpos))) return;

	/* Pick a lesser vault */
	while (TRUE)
	{
		/* Access a random vault record */
		v_ptr = &v_info[rand_int(MAX_V_IDX)];

		/* Accept the first lesser vault */
		if (v_ptr->typ == 7) break;
	}

	/* Try to allocate space for room.  If fails, exit */
	if (!room_alloc(wpos, v_ptr->wid, v_ptr->hgt, FALSE, by0, bx0, &xval, &yval))
	{
//		if (cheat_room) msg_print("Could not allocate this vault here");
		return;
	}

	/* Hack -- Build the vault */
//	build_vault(wpos, yval, xval, v_ptr->hgt, v_ptr->wid, v_text + v_ptr->text);
	build_vault(wpos, yval, xval, v_ptr);
}



/*
 * Type 8 -- greater vaults (see "v_info.txt")
 */
//static void build_type8(struct worldpos *wpos, int yval, int xval)
static void build_type8(struct worldpos *wpos, int by0, int bx0)
{
	vault_type	*v_ptr;
	int xval, yval;
	cave_type **zcave;
	if(!(zcave=getcave(wpos))) return;

	/* Pick a lesser vault */
	while (TRUE)
	{
		/* Access a random vault record */
		v_ptr = &v_info[rand_int(MAX_V_IDX)];

		/* Accept the first greater vault */
		if (v_ptr->typ == 8) break;
	}

	/* Try to allocate space for room.  If fails, exit */
	if (!room_alloc(wpos, v_ptr->wid, v_ptr->hgt, FALSE, by0, bx0, &xval, &yval))
	{
//		if (cheat_room) msg_print("Could not allocate this vault here");
		return;
	}

	/* Hack -- Build the vault */
//	build_vault(wpos, yval, xval, v_ptr->hgt, v_ptr->wid, v_text + v_ptr->text);
	build_vault(wpos, yval, xval, v_ptr);
}


/* XXX XXX Here begins a big lump of ToME cake	- Jir - */
/* XXX XXX part II -- builders */

/*
 * DAG:
 * Build an vertical oval room.
 * For every grid in the possible square, check the distance.
 * If it's less than or == than the radius, make it a room square.
 * If its less, make it a normal grid. If it's == make it an outer
 * wall.
 */
static void build_type9(worldpos *wpos, int by0, int bx0)
{
	int rad, x, y, x0, y0;

	int light = FALSE;
	int dun_level = getlevel(wpos);
	cave_type **zcave;
	if(!(zcave=getcave(wpos))) return;


	/* Occasional light */
	if (randint(dun_level) <= 5) light = TRUE;

	rad = rand_int(10);

	/* Try to allocate space for room.  If fails, exit */
	if (!room_alloc(wpos, rad*2+1, rad*2+1, FALSE, by0, bx0, &x0, &y0)) return;

	for (x = x0 - rad; x <= x0 + rad; x++)
	{
		for (y = y0 - rad; y <= y0 + rad; y++)
		{
			if (distance(y0, x0, y, x) == rad)
			{
				zcave[y][x].info |= (CAVE_ROOM);
				if (light) zcave[y][x].info |= (CAVE_GLOW);

				cave_set_feat(wpos, y, x, feat_wall_outer);
			}

			if (distance(y0, x0, y, x) < rad)
			{
				zcave[y][x].info |= (CAVE_ROOM);
				if (light) zcave[y][x].info |= (CAVE_GLOW);

				place_floor(wpos, y, x);
			}
		}
	}
}


/*
 * Store routine for the fractal cave generator
 * this routine probably should be an inline function or a macro
 */
static void store_height(worldpos *wpos, int x, int y, int x0, int y0, byte val,
	int xhsize, int yhsize, int cutoff)
{
	cave_type **zcave;
	if(!(zcave=getcave(wpos))) return;

	/* Only write to points that are "blank" */
	if (zcave[y+ y0 - yhsize][x + x0 - xhsize].feat != 255) return;

	 /* If on boundary set val > cutoff so walls are not as square */
	if (((x == 0) || (y == 0) || (x == xhsize * 2) || (y == yhsize * 2)) &&
	    (val <= cutoff)) val = cutoff + 1;

	/* Store the value in height-map format */
	/* Meant to be temporary, hence no cave_set_feat */
	zcave[y + y0 - yhsize][x + x0 - xhsize].feat = val;

	return;
}



/*
 * Explanation of the plasma fractal algorithm:
 *
 * A grid of points is created with the properties of a 'height-map'
 * This is done by making the corners of the grid have a random value.
 * The grid is then subdivided into one with twice the resolution.
 * The new points midway between two 'known' points can be calculated
 * by taking the average value of the 'known' ones and randomly adding
 * or subtracting an amount proportional to the distance between those
 * points.  The final 'middle' points of the grid are then calculated
 * by averaging all four of the originally 'known' corner points.  An
 * random amount is added or subtracted from this to get a value of the
 * height at that point.  The scaling factor here is adjusted to the
 * slightly larger distance diagonally as compared to orthogonally.
 *
 * This is then repeated recursively to fill an entire 'height-map'
 * A rectangular map is done the same way, except there are different
 * scaling factors along the x and y directions.
 *
 * A hack to change the amount of correlation between points is done using
 * the grd variable.  If the current step size is greater than grd then
 * the point will be random, otherwise it will be calculated by the
 * above algorithm.  This makes a maximum distance at which two points on
 * the height map can affect each other.
 *
 * How fractal caves are made:
 *
 * When the map is complete, a cut-off value is used to create a cave.
 * Heights below this value are "floor", and heights above are "wall".
 * This also can be used to create lakes, by adding more height levels
 * representing shallow and deep water/ lava etc.
 *
 * The grd variable affects the width of passages.
 * The roug variable affects the roughness of those passages
 *
 * The tricky part is making sure the created cave is connected.  This
 * is done by 'filling' from the inside and only keeping the 'filled'
 * floor.  Walls bounding the 'filled' floor are also kept.  Everything
 * else is converted to the normal granite FEAT_WALL_EXTRA.
 */


/*
 * Note that this uses the cave.feat array in a very hackish way
 * the values are first set to zero, and then each array location
 * is used as a "heightmap"
 * The heightmap then needs to be converted back into the "feat" format.
 *
 * grd=level at which fractal turns on.  smaller gives more mazelike caves
 * roug=roughness level.  16=normal.  higher values make things more
 * convoluted small values are good for smooth walls.
 * size=length of the side of the square cave system.
 */

void generate_hmap(worldpos *wpos, int y0, int x0, int xsiz, int ysiz, int grd,
	int roug, int cutoff)
{
	int xhsize, yhsize, xsize, ysize, maxsize;

	/*
	 * fixed point variables- these are stored as 256 x normal value
	 * this gives 8 binary places of fractional part + 8 places of normal part
	 */
	u16b xstep, xhstep, ystep, yhstep, i, j, diagsize, xxsize, yysize;

	cave_type **zcave;
	if(!(zcave=getcave(wpos))) return;


	/* Redefine size so can change the value if out of range */
	xsize = xsiz;
	ysize = ysiz;

	/* Paranoia about size of the system of caves*/
	if (xsize > 254) xsize = 254;
	if (xsize < 4) xsize = 4;
	if (ysize > 254) ysize = 254;
	if (ysize < 4) ysize = 4;

	/* Get offsets to middle of array */
	xhsize = xsize / 2;
	yhsize = ysize / 2;

	/* Fix rounding problem */
	xsize = xhsize * 2;
	ysize = yhsize * 2;

	/*
	 * Scale factor for middle points:
	 * About sqrt(2)*256 - correct for a square lattice
	 * approximately correct for everything else.
	 */
	diagsize = 362;

	/* Maximum of xsize and ysize */
	maxsize = (xsize > ysize) ? xsize : ysize;

	/* Clear the section */
	for (i = 0; i <= xsize; i++)
	{
		for (j = 0; j <= ysize; j++)
		{
			cave_type *c_ptr;

			/* Access the grid */
			c_ptr = &zcave[j + y0 - yhsize][i + x0 - xhsize];

			/* 255 is a flag for "not done yet" */
			c_ptr->feat = 255;

			/* Clear icky flag because may be redoing the cave */
			c_ptr->info &= ~(CAVE_ICKY);
		}
	}

	/* Set the corner values just in case grd>size. */
	store_height(wpos, 0, 0, x0, y0, maxsize, xhsize, yhsize, cutoff);
	store_height(wpos, 0, ysize, x0, y0, maxsize, xhsize, yhsize, cutoff);
	store_height(wpos, xsize, 0, x0, y0, maxsize, xhsize, yhsize, cutoff);
	store_height(wpos, xsize, ysize, x0, y0, maxsize, xhsize, yhsize, cutoff);

	/* Set the middle square to be an open area. */
	store_height(wpos, xhsize, yhsize, x0, y0, 0, xhsize, yhsize, cutoff);


	/* Initialise the step sizes */
	xstep = xhstep = xsize*256;
	ystep = yhstep = ysize*256;
	xxsize = xsize*256;
	yysize = ysize*256;

	/*
	 * Fill in the rectangle with fractal height data - like the
	 * 'plasma fractal' in fractint
	 */
	while ((xstep/256 > 1) || (ystep/256 > 1))
	{
		/* Halve the step sizes */
		xstep = xhstep;
		xhstep /= 2;
		ystep = yhstep;
		yhstep /= 2;

		/* Middle top to bottom */
		for (i = xhstep; i <= xxsize - xhstep; i += xstep)
		{
			for (j = 0; j <= yysize; j += ystep)
			{
				/* If greater than 'grid' level then is random */
				if (xhstep/256 > grd)
				{
					store_height(wpos, i/256, j/256, x0, y0, randint(maxsize),
					             xhsize, yhsize, cutoff);
				}
				else
				{
					cave_type *l, *r;
					byte val;

					/* Left point */
					l = &zcave[j/256+y0-yhsize][(i-xhstep)/256+x0-xhsize];

					/* Right point */
					r = &zcave[j/256+y0-yhsize][(i+xhstep)/256+x0-xhsize];

					/* Average of left and right points + random bit */
					val = (l->feat + r->feat) / 2 +
						  (randint(xstep/256) - xhstep/256) * roug / 16;

					store_height(wpos, i/256, j/256, x0, y0, val,
					             xhsize, yhsize, cutoff);
				}
			}
		}


		/* Middle left to right */
		for (j = yhstep; j <= yysize - yhstep; j += ystep)
		{
			for (i = 0; i <= xxsize; i += xstep)
			{
				/* If greater than 'grid' level then is random */
				if (xhstep/256 > grd)
				{
					store_height(wpos, i/256, j/256, x0, y0, randint(maxsize),
					             xhsize, yhsize, cutoff);
				}
				else
				{
					cave_type *u, *d;
					byte val;

					/* Up point */
					u = &zcave[(j-yhstep)/256+y0-yhsize][i/256+x0-xhsize];

					/* Down point */
					d = &zcave[(j+yhstep)/256+y0-yhsize][i/256+x0-xhsize];

					/* Average of up and down points + random bit */
					val = (u->feat + d->feat) / 2 +
						  (randint(ystep/256) - yhstep/256) * roug / 16;

					store_height(wpos, i/256, j/256, x0, y0, val,
					             xhsize, yhsize, cutoff);
				}
			}
		}

		/* Center */
		for (i = xhstep; i <= xxsize - xhstep; i += xstep)
		{
			for (j = yhstep; j <= yysize - yhstep; j += ystep)
			{
				/* If greater than 'grid' level then is random */
				if (xhstep/256 > grd)
				{
					store_height(wpos, i/256, j/256, x0, y0, randint(maxsize),
					             xhsize, yhsize, cutoff);
				}
				else
				{
					cave_type *ul, *dl, *ur, *dr;
					byte val;

					/* Up-left point */
					ul = &zcave[(j-yhstep)/256+y0-yhsize][(i-xhstep)/256+x0-xhsize];

					/* Down-left point */
					dl = &zcave[(j+yhstep)/256+y0-yhsize][(i-xhstep)/256+x0-xhsize];

					/* Up-right point */
					ur = &zcave[(j-yhstep)/256+y0-yhsize][(i+xhstep)/256+x0-xhsize];

					/* Down-right point */
					dr = &zcave[(j+yhstep)/256+y0-yhsize][(i+xhstep)/256+x0-xhsize];

					/*
					 * average over all four corners + scale by diagsize to
					 * reduce the effect of the square grid on the shape
					 * of the fractal
					 */
					val = (ul->feat + dl->feat + ur->feat + dr->feat) / 4 +
					      (randint(xstep/256) - xhstep/256) *
						  (diagsize / 16) / 256 * roug;

					store_height(wpos, i/256, j/256, x0, y0, val,
					             xhsize, yhsize ,cutoff);
				}
			}
		}
	}
}


/*
 * Convert from height-map back to the normal Angband cave format
 */
static bool hack_isnt_wall(worldpos *wpos, int y, int x, int cutoff)
{
	cave_type **zcave;
	if(!(zcave=getcave(wpos))) return;

	/* Already done */
	if (zcave[y][x].info & CAVE_ICKY)
	{
		return (FALSE);
	}

	else
	{
		/* Show that have looked at this square */
		zcave[y][x].info |= (CAVE_ICKY);

		/* If less than cutoff then is a floor */
		if (zcave[y][x].feat <= cutoff)
		{
			place_floor(wpos, y, x);
			return (TRUE);
		}

		/* If greater than cutoff then is a wall */
		else
		{
			cave_set_feat(wpos, y, x, feat_wall_outer);
			return (FALSE);
		}
	}
}


/*
 * Quick and nasty fill routine used to find the connected region
 * of floor in the middle of the cave
 */
static void fill_hack(worldpos *wpos, int y0, int x0, int y, int x, int xsize, int ysize,
	int cutoff, int *amount)
{
	int i, j;
	cave_type **zcave;
	if(!(zcave=getcave(wpos))) return;


	/* check 8 neighbours +self (self is caught in the isnt_wall function) */
	for (i = -1; i <= 1; i++)
	{
		for (j = -1; j <= 1; j++)
		{
			/* If within bounds */
			if ((x + i > 0) && (x + i < xsize) &&
			    (y + j > 0) && (y + j < ysize))
			{
				/* If not a wall or floor done before */
				if (hack_isnt_wall(wpos, y + j + y0 - ysize / 2,
				                   x + i + x0 - xsize / 2, cutoff))
				{
					/* then fill from the new point*/
					fill_hack(wpos, y0, x0, y + j, x + i, xsize, ysize,
					          cutoff, amount);

					/* keep tally of size of cave system */
					(*amount)++;
				}
			}

			/* Affect boundary */
			else
			{
				zcave[y0+y+j-ysize/2][x0+x+i-xsize/2].info |= (CAVE_ICKY);
			}
		}
	}
}


bool generate_fracave(worldpos *wpos, int y0, int x0, int xsize, int ysize,
	int cutoff, bool light, bool room)
{
	int x, y, i, amount, xhsize, yhsize;
	cave_type *c_ptr;
	cave_type **zcave;
	if(!(zcave=getcave(wpos))) return;

	/* Offsets to middle from corner */
	xhsize = xsize / 2;
	yhsize = ysize / 2;

	/* Reset tally */
	amount = 0;

	/*
	 * Select region connected to center of cave system
	 * this gets rid of alot of isolated one-sqaures that
	 * can make teleport traps instadeaths...
	 */
	fill_hack(wpos, y0, x0, yhsize, xhsize, xsize, ysize, cutoff, &amount);

	/* If tally too small, try again */
	if (amount < 10)
	{
		/* Too small -- clear area and try again later */
		for (x = 0; x <= xsize; ++x)
		{
			for (y = 0; y < ysize; ++y)
			{
				place_filler(wpos, y0+y-yhsize, x0+x-xhsize);
				zcave[y0+y-yhsize][x0+x-xhsize].info &= ~(CAVE_ICKY|CAVE_ROOM);
			}
		}
		return FALSE;
	}


	/*
	 * Do boundaries -- check to see if they are next to a filled region
	 * If not then they are set to normal granite
	 * If so then they are marked as room walls
	 */
	for (i = 0; i <= xsize; ++i)
	{
		/* Access top boundary grid */
		c_ptr = &zcave[0 + y0 - yhsize][i + x0 - xhsize];

		/* Next to a 'filled' region? -- set to be room walls */
		if (c_ptr->info & CAVE_ICKY)
		{
			cave_set_feat(wpos, 0+y0-yhsize, i+x0-xhsize, feat_wall_outer);

			if (light) c_ptr->info|=(CAVE_GLOW);
			if (room)
			{
				c_ptr->info |= (CAVE_ROOM);
			}
			else
			{
				place_filler(wpos, 0+y0-yhsize, i+x0-xhsize);
			}
		}

		/* Outside of the room -- set to be normal granite */
		else
		{
			place_filler(wpos, 0+y0-yhsize, i+x0-xhsize);
		}

		/* Clear the icky flag -- don't need it any more */
		c_ptr->info &= ~(CAVE_ICKY);


		/* Access bottom boundary grid */
		c_ptr = &zcave[ysize + y0 - yhsize][i + x0 - xhsize];

		/* Next to a 'filled' region? -- set to be room walls */
		if (c_ptr->info & CAVE_ICKY)
		{
			cave_set_feat(wpos, ysize+y0-yhsize, i+x0-xhsize, feat_wall_outer);
			if (light) c_ptr->info |= (CAVE_GLOW);
			if (room)
			{
				c_ptr->info |= (CAVE_ROOM);
			}
			else
			{
				place_filler(wpos, ysize+y0-yhsize, i+x0-xhsize);
			}
		}

		/* Outside of the room -- set to be normal granite */
		else
		{
			place_filler(wpos, ysize+y0-yhsize, i+x0-xhsize);
		}

		/* Clear the icky flag -- don't need it any more */
		c_ptr->info &= ~(CAVE_ICKY);
	}


	/* Do the left and right boundaries minus the corners (done above) */
	for (i = 1; i < ysize; ++i)
	{
		/* Access left boundary grid */
		c_ptr = &zcave[i + y0 - yhsize][0 + x0 - xhsize];

		/* Next to a 'filled' region? -- set to be room walls */
		if (c_ptr->info & CAVE_ICKY)
		{
			cave_set_feat(wpos, i+y0-yhsize, 0+x0-xhsize, feat_wall_outer);
			if (light) c_ptr->info |= (CAVE_GLOW);
			if (room)
			{
				c_ptr->info |= (CAVE_ROOM);
			}
			else
			{
				place_filler(wpos, i+y0-yhsize, 0+x0-xhsize);
			}
		}

		/* Outside of the room -- set to be normal granite */
		else
		{
			place_filler(wpos, i+y0-yhsize, 0+x0-xhsize);
		}

		/* Clear the icky flag -- don't need it any more */
		c_ptr->info &= ~(CAVE_ICKY);


		/* Access left boundary grid */
		c_ptr = &zcave[i + y0 - yhsize][xsize + x0 - xhsize];

		/* Next to a 'filled' region? -- set to be room walls */
		if (c_ptr->info & CAVE_ICKY)
		{
			cave_set_feat(wpos, i+y0-yhsize, xsize+x0-xhsize, feat_wall_outer);
			if (light) c_ptr->info |= (CAVE_GLOW);
			if (room)
			{
				c_ptr->info |= (CAVE_ROOM);
			}
			else
			{
				place_filler(wpos, i+y0-yhsize, xsize+x0-xhsize);
			}
		}

		/* Outside of the room -- set to be normal granite */
		else
		{
			place_filler(wpos, i+y0-yhsize, xsize+x0-xhsize);
		}

		/* Clear the icky flag -- don't need it any more */
		c_ptr->info &= ~(CAVE_ICKY);
	}


	/*
	 * Do the rest: convert back to the normal format
	 * In other variants, may want to check to see if cave.feat< some value
	 * if so, set to be water:- this will make interesting pools etc.
	 * (I don't do this for standard Angband.)
	 */
	for (x = 1; x < xsize; ++x)
	{
		for(y = 1;y < ysize; ++y)
		{
			/* Access the grid */
			c_ptr = &zcave[y + y0 - yhsize][x + x0 - xhsize];

			/* A floor grid to be converted */
//			if ((f_info[c_ptr->feat].flags1 & FF1_FLOOR) &&
			if ((c_ptr->feat == FEAT_FLOOR || c_ptr->feat == FEAT_DEEP_WATER) &&
			    (c_ptr->info & CAVE_ICKY))

			{
				/* Clear the icky flag in the filled region */
				c_ptr->info &= ~(CAVE_ICKY);

				/* Set appropriate flags */
				if (light) c_ptr->info |= (CAVE_GLOW);
				if (room) c_ptr->info |= (CAVE_ROOM);
			}

			/* A wall grid to be convereted */
			else if ((c_ptr->feat == feat_wall_outer) &&
			         (c_ptr->info & CAVE_ICKY))
			{
				/* Clear the icky flag in the filled region */
				c_ptr->info &= ~(CAVE_ICKY);

				/* Set appropriate flags */
				if (light) c_ptr->info |= (CAVE_GLOW);
				if (room)
				{
					c_ptr->info |= (CAVE_ROOM);
				}
				else
				{
					place_filler(wpos, y+y0-yhsize, x+x0-xhsize);
				}
			}

			/* None of the above -- clear the unconnected regions */
			else
			{
				place_filler(wpos, y+y0-yhsize, x+x0-xhsize);
				c_ptr->info &= ~(CAVE_ICKY|CAVE_ROOM);
			}
		}
	}

	/*
	 * XXX XXX XXX There is a slight problem when tunnels pierce the caves:
	 * Extra doors appear inside the system.  (Its not very noticeable though.)
	 * This can be removed by "filling" from the outside in.  This allows
	 * a separation from FEAT_WALL_OUTER with FEAT_WALL_INNER.  (Internal
	 * walls are  F.W.OUTER instead.)
	 * The extra effort for what seems to be only a minor thing (even
	 * non-existant if you think of the caves not as normal rooms, but as
	 * holes in the dungeon), doesn't seem worth it.
	 */

	return (TRUE);
}


/*
 * Makes a cave system in the center of the dungeon
 */
static void build_cavern(worldpos *wpos)
{
	int grd, roug, cutoff, xsize, ysize, x0, y0;
	bool done, light, room;
	int dun_level = getlevel(wpos);

	light = done = room = FALSE;
	if (dun_level <= randint(25)) light = TRUE;

	/* Make a cave the size of the dungeon */
#if 0
	xsize = cur_wid - 1;
	ysize = cur_hgt - 1;
#endif	// 0
	ysize = dun->l_ptr->hgt - 1;
	xsize = dun->l_ptr->wid - 1;
	x0 = xsize / 2;
	y0 = ysize / 2;

	/* Paranoia: make size even */
	xsize = x0 * 2;
	ysize = y0 * 2;

	while (!done)
	{
		/* Testing values for these parameters: feel free to adjust */
		grd = 2^(randint(4) + 4);

		/* Want average of about 16 */
		roug =randint(8) * randint(4);

		/* About size/2 */
		cutoff = xsize / 2;

		 /* Make it */
		generate_hmap(wpos, y0, x0, xsize, ysize, grd, roug, cutoff);

		/* Convert to normal format+ clean up*/
		done=generate_fracave(wpos, y0, x0, xsize, ysize, cutoff, light, room);
	}
}


/*
 * Driver routine to create fractal cave system
 */
static void build_type10(worldpos *wpos, int by0, int bx0)
{
	int grd, roug, cutoff, xsize, ysize, y0, x0;

	bool done, light, room;
	int dun_level = getlevel(wpos);

	/* Get size: note 'Evenness'*/
	xsize = randint(22) * 2 + 6;
	ysize = randint(15) * 2 + 6;

	/* Try to allocate space for room.  If fails, exit */
	if (!room_alloc(wpos, xsize+1, ysize+1, FALSE, by0, bx0, &x0, &y0)) return;

	light = done = FALSE;
	room = TRUE;

	if (dun_level <= randint(25)) light = TRUE;

	while (!done)
	{
		/*
		 * Note: size must be even or there are rounding problems
		 * This causes the tunnels not to connect properly to the room
		 */

		/* Testing values for these parameters feel free to adjust */
		grd = 2^(randint(4));

		/* Want average of about 16 */
		roug = randint(8) * randint(4);

		/* About size/2 */
		cutoff = randint(xsize / 4) + randint(ysize / 4) +
		         randint(xsize / 4) + randint(ysize / 4);

		/* Make it */
		generate_hmap(wpos, y0, x0, xsize, ysize, grd, roug, cutoff);

		/* Convert to normal format + clean up*/
		done = generate_fracave(wpos, y0, x0, xsize, ysize, cutoff, light, room);
	}
}


/*
 * Random vault generation from Z 2.5.1
 */

/*
 * Make a very small room centred at (x0, y0)
 *
 * This is used in crypts, and random elemental vaults.
 *
 * Note - this should be used only on allocated regions
 * within another room.
 */
static void build_small_room(worldpos *wpos, int x0, int y0)
{
	build_rectangle(wpos, y0 - 1, x0 - 1, y0 + 1, x0 + 1, feat_wall_inner, CAVE_ROOM);

	/* Place a secret door on one side */
	switch (rand_int(4))
	{
		case 0:
		{
			place_secret_door(wpos, y0, x0 - 1);
			break;
		}

		case 1:
		{
			place_secret_door(wpos, y0, x0 + 1);
			break;
		}

		case 2:
		{
			place_secret_door(wpos, y0 - 1, x0);
			break;
		}

		case 3:
		{
			place_secret_door(wpos, y0 + 1, x0);
			break;
		}
	}

	/* Add inner open space */
	place_floor(wpos, y0, x0);
}


/*
 * Add a door to a location in a random vault
 *
 * Note that range checking has to be done in the calling routine.
 *
 * The doors must be INSIDE the allocated region.
 */
static void add_door(worldpos *wpos, int x, int y)
{
	cave_type **zcave;
	if(!(zcave=getcave(wpos))) return;

	/* Need to have a wall in the center square */
	if (zcave[y][x].feat != feat_wall_outer) return;

	/*
	 * Look at:
	 *  x#x
	 *  .#.
	 *  x#x
	 *
	 *  where x=don't care
	 *  .=floor, #=wall
	 */

	if (get_is_floor(wpos, x, y - 1) && get_is_floor(wpos, x, y + 1) &&
	    (zcave[y][x - 1].feat == feat_wall_outer) &&
	    (zcave[y][x + 1].feat == feat_wall_outer))
	{
		/* secret door */
		place_secret_door(wpos, y, x);

		/* set boundarys so don't get wide doors */
		place_filler(wpos, y, x - 1);
		place_filler(wpos, y, x + 1);
	}


	/*
	 * Look at:
	 *  x#x
	 *  .#.
	 *  x#x
	 *
	 *  where x = don't care
	 *  .=floor, #=wall
	 */
	if ((zcave[y - 1][x].feat == feat_wall_outer) &&
	    (zcave[y + 1][x].feat == feat_wall_outer) &&
	    get_is_floor(wpos, x - 1, y) && get_is_floor(wpos, x + 1, y))
	{
		/* secret door */
		place_secret_door(wpos, y, x);

		/* set boundarys so don't get wide doors */
		place_filler(wpos, y - 1, x);
		place_filler(wpos, y + 1, x);
	}
}


/*
 * Fill the empty areas of a room with treasure and monsters.
 */
static void fill_treasure(worldpos *wpos, int x1, int x2, int y1, int y2, int difficulty)
{
	int x, y, cx, cy, size;
	s32b value;
	cave_type **zcave;
	int dun_level = getlevel(wpos);
	if(!(zcave=getcave(wpos))) return;


	/* center of room:*/
	cx = (x1 + x2) / 2;
	cy = (y1 + y2) / 2;

	/* Rough measure of size of vault= sum of lengths of sides */
	size = abs(x2 - x1) + abs(y2 - y1);

	for (x = x1; x <= x2; x++)
	{
		for (y = y1; y <= y2; y++)
		{
			/*
			 * Thing added based on distance to center of vault
			 * Difficulty is 1-easy to 10-hard
			 */
			value = (((s32b)distance(cx, cy, x, y) * 100) / size) +
			        randint(10) - difficulty;

			/* Hack -- Empty square part of the time */
			if ((randint(100) - difficulty * 3) > 50) value = 20;

			 /* If floor, shallow water or lava */
			if (get_is_floor(wpos, x, y) ||
			    (zcave[y][x].feat == FEAT_DEEP_WATER))

#if 0
			    (cave[y][x].feat == FEAT_SHAL_WATER) ||
			    (cave[y][x].feat == FEAT_SHAL_LAVA))
#endif	// 0
			{
				/* The smaller 'value' is, the better the stuff */
				if (value < 0)
				{
					/* Meanest monster + treasure */
					monster_level = dun_level + 40;
					place_monster(wpos, y, x, TRUE, TRUE);
					monster_level = dun_level;
					object_level = dun_level + 20;
					place_object(wpos, y, x, TRUE, FALSE, default_obj_theme);
					object_level = dun_level;
				}
				else if (value < 5)
				{
					/* Mean monster +treasure */
					monster_level = dun_level + 20;
					place_monster(wpos, y, x, TRUE, TRUE);
					monster_level = dun_level;
					object_level = dun_level + 10;
					place_object(wpos, y, x, TRUE, FALSE, default_obj_theme);
					object_level = dun_level;
				}
				else if (value < 10)
				{
					/* Monster */
					monster_level = dun_level + 9;
					place_monster(wpos, y, x, TRUE, TRUE);
					monster_level = dun_level;
				}
				else if (value < 17)
				{
					/* Intentional Blank space */

					/*
					 * (Want some of the vault to be empty
					 * so have room for group monsters.
					 * This is used in the hack above to lower
					 * the density of stuff in the vault.)
					 */
				}
				else if (value < 23)
				{
					/* Object or trap */
					if (rand_int(100) < 25)
					{
						place_object(wpos, y, x, FALSE, FALSE, default_obj_theme);
					}
					// else
					if (rand_int(100) < 75)
					{
						place_trap(wpos, y, x, 0);
					}
				}
				else if (value < 30)
				{
					/* Monster and trap */
					monster_level = dun_level + 5;
					place_monster(wpos, y, x, TRUE, TRUE);
					monster_level = dun_level;
					place_trap(wpos, y, x, 0);
				}
				else if (value < 40)
				{
					/* Monster or object */
					if (rand_int(100) < 50)
					{
						monster_level = dun_level + 3;
						place_monster(wpos, y, x, TRUE, TRUE);
						monster_level = dun_level;
					}
					if (rand_int(100) < 50)
					{
						object_level = dun_level + 7;
						place_object(wpos, y, x, FALSE, FALSE, default_obj_theme);
						object_level = dun_level;
					}
				}
				else if (value < 50)
				{
					/* Trap */
					place_trap(wpos, y, x, 0);
				}
				else
				{
					/* Various Stuff */

					/* 20% monster, 40% trap, 20% object, 20% blank space */
					if (rand_int(100) < 20)
					{
						place_monster(wpos, y, x, TRUE, TRUE);
					}
					else if (rand_int(100) < 50)
					{
						place_trap(wpos, y, x, 0);
					}
					else if (rand_int(100) < 50)
					{
						place_object(wpos, y, x, FALSE, FALSE, default_obj_theme);
					}
				}

			}
		}
	}
}


/*
 * Creates a random vault that looks like a collection of bubbles
 *
 * It works by getting a set of coordinates that represent the center of
 * each bubble.  The entire room is made by seeing which bubble center is
 * closest. If two centers are equidistant then the square is a wall,
 * otherwise it is a floor. The only exception is for squares really
 * near a center, these are always floor.
 * (It looks better than without this check.)
 *
 * Note: If two centers are on the same point then this algorithm will create a
 * blank bubble filled with walls. - This is prevented from happening.
 */

#define BUBBLENUM 10 /* number of bubbles */

static void build_bubble_vault(worldpos *wpos, int x0, int y0, int xsize, int ysize)
{
	/* array of center points of bubbles */
	coord center[BUBBLENUM];

	int i, j, k, x = 0, y = 0;
	u16b min1, min2, temp;
	bool done;

	/* Offset from center to top left hand corner */
	int xhsize = xsize / 2;
	int yhsize = ysize / 2;

	cave_type **zcave;
//	int dun_level = getlevel(wpos);
	if(!(zcave=getcave(wpos))) return;

//	if (cheat_room) msg_print("Bubble Vault");

	/* Allocate center of bubbles */
	center[0].x = randint(xsize - 3) + 1;
	center[0].y = randint(ysize - 3) + 1;

	for (i = 1; i < BUBBLENUM; i++)
	{
		done = FALSE;

		/* Get center and check to see if it is unique */
		for (k = 0; !done && (k < 2000); k++)
		{
			done = TRUE;

			x = randint(xsize - 3) + 1;
			y = randint(ysize - 3) + 1;

			for (j = 0; j < i; j++)
			{
				/* Rough test to see if there is an overlap */
				if ((x == center[j].x) || (y == center[j].y)) done = FALSE;
			}
		}

		/* Too many failures */
		if (k >= 2000) return;

		center[i].x = x;
		center[i].y = y;
	}

	build_rectangle(wpos, y0 - yhsize, x0 - xhsize,
	                y0 - yhsize + ysize - 1, x0 - xhsize + xsize - 1,
	                feat_wall_outer, CAVE_ROOM | CAVE_ICKY);

	/* Fill in middle with bubbles */
	for (x = 1; x < xsize - 1; x++)
	{
		for (y = 1; y < ysize - 1; y++)
		{
			cave_type *c_ptr;

			/* Get distances to two closest centers */

			/* Initialise */
			min1 = distance(x, y, center[0].x, center[0].y);
			min2 = distance(x, y, center[1].x, center[1].y);

			if (min1 > min2)
			{
				/* Swap if in wrong order */
				temp = min1;
				min1 = min2;
				min2 = temp;
			}

			/* Scan the rest */
			for (i = 2; i < BUBBLENUM; i++)
			{
				temp = distance(x, y, center[i].x, center[i].y);

				if (temp < min1)
				{
					/* Smallest */
					min2 = min1;
					min1 = temp;
				}
				else if (temp < min2)
				{
					/* Second smallest */
					min2 = temp;
				}
			}

			/* Access the grid */
			c_ptr = &zcave[y + y0 - yhsize][x + x0 - xhsize];

			/*
			 * Boundary at midpoint+ not at inner region of bubble
			 *
			 * SCSCSC: was feat_wall_outer
			 */
			if (((min2 - min1) <= 2) && (!(min1 < 3)))
			{
				place_filler(wpos, y+y0-yhsize, x+x0-xhsize);
			}

			/* Middle of a bubble */
			else
			{
				place_floor(wpos, y+y0-yhsize, x+x0-xhsize);
			}

			/* Clean up rest of flags */
			c_ptr->info |= (CAVE_ROOM | CAVE_ICKY);
		}
	}

	/* Try to add some random doors */
	for (i = 0; i < 500; i++)
	{
		x = randint(xsize - 3) - xhsize + x0 + 1;
		y = randint(ysize - 3) - yhsize + y0 + 1;
		add_door(wpos, x, y);
	}

	/* Fill with monsters and treasure, low difficulty */
	fill_treasure(wpos, x0 - xhsize + 1, x0 - xhsize + xsize - 2,
	              y0 - yhsize + 1, y0 - yhsize + ysize - 2, randint(5));
}


/*
 * Convert FEAT_WALL_EXTRA (used by random vaults) to normal dungeon wall
 */
static void convert_extra(worldpos *wpos, int y1, int x1, int y2, int x2)
{
	int x, y;

	cave_type **zcave;
//	int dun_level = getlevel(wpos);
	if(!(zcave=getcave(wpos))) return;

	for (x = x1; x <= x2; x++)
	{
		for (y = y1; y <= y2; y++)
		{
			if (zcave[y][x].feat == FEAT_WALL_OUTER)
			{
				place_filler(wpos, y, x);
			}
		}
	}
}


/*
 * Overlay a rectangular room given its bounds
 *
 * This routine is used by build_room_vault (hence FEAT_WALL_OUTER)
 * The area inside the walls is not touched: only granite is removed
 * and normal walls stay
 */
static void build_room(worldpos *wpos, int x1, int x2, int y1, int y2)
{
	int x, y, xsize, ysize, temp;

	cave_type **zcave;
//	int dun_level = getlevel(wpos);
	if(!(zcave=getcave(wpos))) return;

	/* Check if rectangle has no width */
	if ((x1 == x2) || (y1 == y2)) return;

	/* initialize */
	if (x1 > x2)
	{
		/* Swap boundaries if in wrong order */
		temp = x1;
		x1 = x2;
		x2 = temp;
	}

	if (y1 > y2)
	{
		/* Swap boundaries if in wrong order */
		temp = y1;
		y1 = y2;
		y2 = temp;
	}

	/* Get total widths */
	xsize = x2 - x1;
	ysize = y2 - y1;

	build_rectangle(wpos, y1, x1, y2, x2, feat_wall_outer, CAVE_ROOM | CAVE_ICKY);

	/* Middle */
	for (x = 1; x < xsize; x++)
	{
		for (y = 1; y < ysize; y++)
		{
			if (zcave[y1 + y][x1 + x].feat == FEAT_WALL_OUTER)
			{
				/* Clear the untouched region */
				place_floor(wpos, y1 + y, x1 + x);
				zcave[y1 + y][x1 + x].info |= (CAVE_ROOM | CAVE_ICKY);
			}
			else
			{
				/* Make it a room -- but don't touch */
				zcave[y1 + y][x1 + x].info |= (CAVE_ROOM | CAVE_ICKY);
			}
		}
	}
}


/*
 * Create a random vault that looks like a collection of overlapping rooms
 */
static void build_room_vault(worldpos *wpos, int x0, int y0, int xsize, int ysize)
{
	int i, x1, x2, y1, y2, xhsize, yhsize;

	cave_type **zcave;
//	int dun_level = getlevel(wpos);
	if(!(zcave=getcave(wpos))) return;

	/* Get offset from center */
	xhsize = xsize / 2;
	yhsize = ysize / 2;

//	if (cheat_room) msg_print("Room Vault");

	/* Fill area so don't get problems with arena levels */
	for (x1 = 0; x1 <= xsize; x1++)
	{
		int x = x0 - xhsize + x1;

		for (y1 = 0; y1 <= ysize; y1++)
		{
			int y = y0 - yhsize + y1;

			cave_set_feat(wpos, y, x, FEAT_WALL_OUTER);
			zcave[y][x].info &= ~(CAVE_ICKY);
		}
	}

	/* Add ten random rooms */
	for (i = 0; i < 10; i++)
	{
		x1 = randint(xhsize) * 2 + x0 - xhsize;
		x2 = randint(xhsize) * 2 + x0 - xhsize;
		y1 = randint(yhsize) * 2 + y0 - yhsize;
		y2 = randint(yhsize) * 2 + y0 - yhsize;

		build_room(wpos, x1, x2, y1, y2);
	}

	convert_extra(wpos, y0 - yhsize, x0 - xhsize, y0 - yhsize + ysize,
		      x0 - xhsize + xsize);

	/* Add some random doors */
	for (i = 0; i < 500; i++)
	{
		x1 = randint(xsize - 2) - xhsize + x0 + 1;
		y1 = randint(ysize - 2) - yhsize + y0 + 1;
		add_door(wpos, x1, y1);
	}

	/* Fill with monsters and treasure, high difficulty */
	fill_treasure(wpos, x0 - xhsize + 1, x0 - xhsize + xsize - 1,
	              y0 - yhsize + 1, y0 - yhsize + ysize - 1, randint(5) + 5);
}


/*
 * Create a random vault out of a fractal cave
 */
static void build_cave_vault(worldpos *wpos, int x0, int y0, int xsiz, int ysiz)
{
	int grd, roug, cutoff, xhsize, yhsize, xsize, ysize, x, y;
	bool done, light, room;

	cave_type **zcave;
//	int dun_level = getlevel(wpos);
	if(!(zcave=getcave(wpos))) return;

	/* Round to make sizes even */
	xhsize = xsiz / 2;
	yhsize = ysiz / 2;
	xsize = xhsize * 2;
	ysize = yhsize * 2;

//	if (cheat_room) msg_print("Cave Vault");

	light = done = FALSE;
	room = TRUE;

	while (!done)
	{
		/* Testing values for these parameters feel free to adjust */
		grd = 2^rand_int(4);

		/* Want average of about 16 */
		roug = randint(8) * randint(4);

		/* About size/2 */
		cutoff = randint(xsize / 4) + randint(ysize / 4) +
			 randint(xsize / 4) + randint(ysize / 4);

		/* Make it */
		generate_hmap(wpos, y0, x0, xsize, ysize, grd, roug, cutoff);

		/* Convert to normal format + clean up */
		done = generate_fracave(wpos, y0, x0, xsize, ysize, cutoff, light, room);
	}

	/* Set icky flag because is a vault */
	for (x = 0; x <= xsize; x++)
	{
		for (y = 0; y <= ysize; y++)
		{
			zcave[y0 - yhsize + y][x0 - xhsize + x].info |= CAVE_ICKY;
		}
	}

	/* Fill with monsters and treasure, low difficulty */
	fill_treasure(wpos, x0 - xhsize + 1, x0 - xhsize + xsize - 1,
	              y0 - yhsize + 1, y0 - yhsize + ysize - 1, randint(5));
}


/*
 * Maze vault -- rectangular labyrinthine rooms
 *
 * maze vault uses two routines:
 *    r_visit - a recursive routine that builds the labyrinth
 *    build_maze_vault - a driver routine that calls r_visit and adds
 *                   monsters, traps and treasure
 *
 * The labyrinth is built by creating a spanning tree of a graph.
 * The graph vertices are at
 *    (x, y) = (2j + x1, 2k + y1)   j = 0,...,m-1    k = 0,...,n-1
 * and the edges are the vertical and horizontal nearest neighbors.
 *
 * The spanning tree is created by performing a suitably randomized
 * depth-first traversal of the graph. The only adjustable parameter
 * is the rand_int(3) below; it governs the relative density of
 * twists and turns in the labyrinth: smaller number, more twists.
 */
static void r_visit(worldpos *wpos, int y1, int x1, int y2, int x2,
		    int node, int dir, int *visited)
{
	int i, j, m, n, temp, x, y, adj[4];

	/* Dimensions of vertex array */
	m = (x2 - x1) / 2 + 1;
	n = (y2 - y1) / 2 + 1;

	/* Mark node visited and set it to a floor */
	visited[node] = 1;
	x = 2 * (node % m) + x1;
	y = 2 * (node / m) + y1;
	place_floor(wpos, y, x);

	/* Setup order of adjacent node visits */
	if (rand_int(3) == 0)
	{
		/* Pick a random ordering */
		for (i = 0; i < 4; i++)
		{
			adj[i] = i;
		}
		for (i = 0; i < 4; i++)
		{
			j = rand_int(4);
			temp = adj[i];
			adj[i] = adj[j];
			adj[j] = temp;
		}
		dir = adj[0];
	}
	else
	{
		/* Pick a random ordering with dir first */
		adj[0] = dir;
		for (i = 1; i < 4; i++)
		{
			adj[i] = i;
		}
		for (i = 1; i < 4; i++)
		{
			j = 1 + rand_int(3);
			temp = adj[i];
			adj[i] = adj[j];
			adj[j] = temp;
		}
	}

	for (i = 0; i < 4; i++)
	{
		switch (adj[i])
		{
			/* (0,+) - check for bottom boundary */
			case 0:
			{
				if ((node / m < n - 1) && (visited[node + m] == 0))
				{
					place_floor(wpos, y + 1, x);
					r_visit(wpos, y1, x1, y2, x2, node + m, dir, visited);
				}
				break;
			}

			/* (0,-) - check for top boundary */
			case 1:
			{
				if ((node / m > 0) && (visited[node - m] == 0))
				{
					place_floor(wpos, y - 1, x);
					r_visit(wpos, y1, x1, y2, x2, node - m, dir, visited);
				}
				break;
			}

			/* (+,0) - check for right boundary */
			case 2:
			{
				if ((node % m < m - 1) && (visited[node + 1] == 0))
				{
					place_floor(wpos, y, x + 1);
					r_visit(wpos, y1, x1, y2, x2, node + 1, dir, visited);
				}
				break;
			}

			/* (-,0) - check for left boundary */
			case 3:
			{
				if ((node % m > 0) && (visited[node - 1] == 0))
				{
					place_floor(wpos, y, x - 1);
					r_visit(wpos, y1, x1, y2, x2, node - 1, dir, visited);
				}
				break;
			}
		}
	}
}


static void build_maze_vault(worldpos *wpos, int x0, int y0, int xsize, int ysize)
{
	int y, x, dy, dx;
	int y1, x1, y2, x2;
	int i, m, n, num_vertices, *visited;
	bool light;
	cave_type *c_ptr;

	cave_type **zcave;
	int dun_level = getlevel(wpos);
	if(!(zcave=getcave(wpos))) return;


//	if (cheat_room) msg_print("Maze Vault");

	/* Choose lite or dark */
	light = (dun_level <= randint(25));

	/* Pick a random room size - randomized by calling routine */
	dy = ysize / 2 - 1;
	dx = xsize / 2 - 1;

	y1 = y0 - dy;
	x1 = x0 - dx;
	y2 = y0 + dy;
	x2 = x0 + dx;

	/* Generate the room */
	for (y = y1 - 1; y <= y2 + 1; y++)
	{
		for (x = x1 - 1; x <= x2 + 1; x++)
		{
			c_ptr = &zcave[y][x];

			c_ptr->info |= (CAVE_ROOM | CAVE_ICKY);

			if ((x == x1 - 1) || (x == x2 + 1) ||
			    (y == y1 - 1) || (y == y2 + 1))
			{
				cave_set_feat(wpos, y, x, feat_wall_outer);
			}
			else
			{
				cave_set_feat(wpos, y, x, feat_wall_inner);
			}
			if (light) c_ptr->info |= (CAVE_GLOW);
		}
	}

	/* Dimensions of vertex array */
	m = dx + 1;
	n = dy + 1;
	num_vertices = m * n;

	/* Allocate an array for visited vertices */
	C_MAKE(visited, num_vertices, int);

	/* Initialise array of visited vertices */
	for (i = 0; i < num_vertices; i++)
	{
		visited[i] = 0;
	}

	/* Traverse the graph to create a spaning tree, pick a random root */
	r_visit(wpos, y1, x1, y2, x2, rand_int(num_vertices), 0, visited);

	/* Fill with monsters and treasure, low difficulty */
	fill_treasure(wpos, x1, x2, y1, y2, randint(5));

	/* Free the array for visited vertices */
	C_FREE(visited, num_vertices, int);
}


/*
 * Build a "mini" checkerboard vault
 *
 * This is done by making a permanent wall maze and setting
 * the diagonal sqaures of the checker board to be granite.
 * The vault has two entrances on opposite sides to guarantee
 * a way to get in even if the vault abuts a side of the dungeon.
 */
static void build_mini_c_vault(worldpos *wpos, int x0, int y0, int xsize, int ysize)
{
	int dy, dx;
	int y1, x1, y2, x2, y, x, total;
	int i, m, n, num_vertices;
	int *visited;

	cave_type **zcave;
//	int dun_level = getlevel(wpos);
	if(!(zcave=getcave(wpos))) return;

//	if (cheat_room) msg_print("Mini Checker Board Vault");

	/* Pick a random room size */
	dy = ysize / 2 - 1;
	dx = xsize / 2 - 1;

	y1 = y0 - dy;
	x1 = x0 - dx;
	y2 = y0 + dy;
	x2 = x0 + dx;


	/* Generate the room */
	for (y = y1 - 1; y <= y2 + 1; y++)
	{
		for (x = x1 - 1; x <= x2 + 1; x++)
		{
			zcave[y][x].info |= (CAVE_ROOM | CAVE_ICKY);

			/* Permanent walls */
			cave_set_feat(wpos, y, x, FEAT_PERM_INNER);
		}
	}


	/* Dimensions of vertex array */
	m = dx + 1;
	n = dy + 1;
	num_vertices = m * n;

	/* Allocate an array for visited vertices */
	C_MAKE(visited, num_vertices, int);

	/* Initialise array of visited vertices */
	for (i = 0; i < num_vertices; i++)
	{
		visited[i] = 0;
	}

	/* Traverse the graph to create a spannng tree, pick a random root */
	r_visit(wpos, y1, x1, y2, x2, rand_int(num_vertices), 0, visited);

	/* Make it look like a checker board vault */
	for (x = x1; x <= x2; x++)
	{
		for (y = y1; y <= y2; y++)
		{
			total = x - x1 + y - y1;

			/* If total is odd and is a floor, then make a wall */
			if ((total % 2 == 1) && get_is_floor(wpos, x, y))
			{
				cave_set_feat(wpos, y, x, feat_wall_inner);
			}
		}
	}

	/* Make a couple of entrances */
	if (rand_int(2) == 0)
	{
		/* Left and right */
		y = randint(dy) + dy / 2;
		cave_set_feat(wpos, y1 + y, x1 - 1, feat_wall_outer);
		cave_set_feat(wpos, y1 + y, x2 + 1, feat_wall_outer);
	}
	else
	{
		/* Top and bottom */
		x = randint(dx) + dx / 2;
		cave_set_feat(wpos, y1 - 1, x1 + x, feat_wall_outer);
		cave_set_feat(wpos, y2 + 1, x1 + x, feat_wall_outer);
	}

	/* Fill with monsters and treasure, highest difficulty */
	fill_treasure(wpos, x1, x2, y1, y2, 10);

	/* Free the array for visited vertices */
	C_FREE(visited, num_vertices, int);
}


/*
 * Build a town/ castle by using a recursive algorithm.
 * Basically divide each region in a probalistic way to create
 * smaller regions.  When the regions get too small stop.
 *
 * The power variable is a measure of how well defended a region is.
 * This alters the possible choices.
 */
static void build_recursive_room(worldpos *wpos, int x1, int y1, int x2, int y2, int power)
{
	int xsize, ysize;
	int x, y;
	int choice;

	/* Temp variables */
	int t1, t2, t3, t4;

	xsize = x2 - x1;
	ysize = y2 - y1;

	if ((power < 3) && (xsize > 12) && (ysize > 12))
	{
		/* Need outside wall +keep */
		choice = 1;
	}
	else
	{
		if (power < 10)
		{
			/* Make rooms + subdivide */
			if ((randint(10) > 2) && (xsize < 8) && (ysize < 8))
			{
				choice = 4;
			}
			else
			{
				choice = randint(2) + 1;
			}
		}
		else
		{
			/* Mostly subdivide */
			choice = randint(3) + 1;
		}
	}

	/* Based on the choice made above, do something */
	switch (choice)
	{
		/* Outer walls */
		case 1:
		{
			/* Top and bottom */
			for (x = x1; x <= x2; x++)
			{
				cave_set_feat(wpos, y1, x, feat_wall_outer);
				cave_set_feat(wpos, y2, x, feat_wall_outer);
			}

			/* Left and right */
			for (y = y1 + 1; y < y2; y++)
			{
				cave_set_feat(wpos, y, x1, feat_wall_outer);
				cave_set_feat(wpos, y, x2, feat_wall_outer);
			}

			/* Make a couple of entrances */
			if (rand_int(2) == 0)
			{
				/* Left and right */
				y = randint(ysize) + y1;
				place_floor(wpos, y, x1);
				place_floor(wpos, y, x2);
			}
			else
			{
				/* Top and bottom */
				x = randint(xsize) + x1;
				place_floor(wpos, y1, x);
				place_floor(wpos, y2, x);
			}

			/* Select size of keep */
			t1 = randint(ysize / 3) + y1;
			t2 = y2 - randint(ysize / 3);
			t3 = randint(xsize / 3) + x1;
			t4 = x2 - randint(xsize / 3);

			/* Do outside areas */

			/* Above and below keep */
			build_recursive_room(wpos, x1 + 1, y1 + 1, x2 - 1, t1, power + 1);
			build_recursive_room(wpos, x1 + 1, t2, x2 - 1, y2, power + 1);

			/* Left and right of keep */
			build_recursive_room(wpos, x1 + 1, t1 + 1, t3, t2 - 1, power + 3);
			build_recursive_room(wpos, t4, t1 + 1, x2 - 1, t2 - 1, power + 3);

			/* Make the keep itself: */
			x1 = t3;
			x2 = t4;
			y1 = t1;
			y2 = t2;
			xsize = x2 - x1;
			ysize = y2 - y1;
			power += 2;

			/* Fall through */
		}

		/* Try to build a room */
		case 4:
		{
			if ((xsize < 3) || (ysize < 3))
			{
				for (y = y1; y < y2; y++)
				{
					for (x = x1; x < x2; x++)
					{
						cave_set_feat(wpos, y, x, feat_wall_inner);
					}
				}

				/* Too small */
				return;
			}

			/* Make outside walls */

			/* Top and bottom */
			for (x = x1 + 1; x <= x2 - 1; x++)
			{
				cave_set_feat(wpos, y1 + 1, x, feat_wall_inner);
				cave_set_feat(wpos, y2 - 1, x, feat_wall_inner);
			}

			/* Left and right */
			for (y = y1 + 1; y <= y2 - 1; y++)
			{
				cave_set_feat(wpos, y, x1 + 1, feat_wall_inner);
				cave_set_feat(wpos, y, x2 - 1, feat_wall_inner);
			}

			/* Make a door */
			y = randint(ysize - 3) + y1 + 1;

			if (rand_int(2) == 0)
			{
				/* Left */
				place_floor(wpos, y, x1 + 1);
			}
			else
			{
				/* Right */
				place_floor(wpos, y, x2 - 1);
			}

			/* Build the room */
			build_recursive_room(wpos, x1 + 2, y1 + 2, x2 - 2, y2 - 2, power + 3);

			break;
		}

		/* Try and divide vertically */
		case 2:
		{
			if (xsize < 3)
			{
				/* Too small */
				for (y = y1; y < y2; y++)
				{
					for (x = x1; x < x2; x++)
					{
						cave_set_feat(wpos, y, x, feat_wall_inner);
					}
				}
				return;
			}

			t1 = randint(xsize - 2) + x1 + 1;
			build_recursive_room(wpos, x1, y1, t1, y2, power - 2);
			build_recursive_room(wpos, t1 + 1, y1, x2, y2, power - 2);

			break;
		}

		/* Try and divide horizontally */
		case 3:
		{
			if (ysize < 3)
			{
				/* Too small */
				for (y = y1; y < y2; y++)
				{
					for (x = x1; x < x2; x++)
					{
						cave_set_feat(wpos, y, x, feat_wall_inner);
					}
				}
				return;
			}

			t1 = randint(ysize - 2) + y1 + 1;
			build_recursive_room(wpos, x1, y1, x2, t1, power - 2);
			build_recursive_room(wpos, x1, t1 + 1, x2, y2, power - 2);

			break;
		}
	}
}


/*
 * Build a castle
 *
 * Clear the region and call the recursive room routine.
 *
 * This makes a vault that looks like a castle or city in the dungeon.
 */
static void build_castle_vault(worldpos *wpos, int x0, int y0, int xsize, int ysize)
{
	int dy, dx;
	int y1, x1, y2, x2;
	int y, x;

	cave_type **zcave;
//	int dun_level = getlevel(wpos);
	if(!(zcave=getcave(wpos))) return;

	/* Pick a random room size */
	dy = ysize / 2 - 1;
	dx = xsize / 2 - 1;

	y1 = y0 - dy;
	x1 = x0 - dx;
	y2 = y0 + dy;
	x2 = x0 + dx;

//	if (cheat_room) msg_print("Castle Vault");

	/* Generate the room */
	for (y = y1 - 1; y <= y2 + 1; y++)
	{
		for (x = x1 - 1; x <= x2 + 1; x++)
		{
			zcave[y][x].info |= (CAVE_ROOM | CAVE_ICKY);

			/* Make everything a floor */
			place_floor(wpos, y, x);
		}
	}

	/* Make the castle */
	build_recursive_room(wpos, x1, y1, x2, y2, randint(5));

	/* Fill with monsters and treasure, low difficulty */
	fill_treasure(wpos, x1, x2, y1, y2, randint(3));
}


/*
 * Add outer wall to a floored region
 *
 * Note: no range checking is done so must be inside dungeon
 * This routine also stomps on doors
 */
static void add_outer_wall(worldpos *wpos, int x, int y, int light, int x1, int y1,
			   int x2, int y2)
{
	int i, j;

	cave_type **zcave;
//	int dun_level = getlevel(wpos);
	if(!(zcave=getcave(wpos))) return;

	if (!in_bounds(y, x)) return;	/* XXX */

	/*
	 * Hack -- Check to see if square has been visited before
	 * if so, then exit (use room flag to do this)
	 */
	if (zcave[y][x].info & CAVE_ROOM) return;

	/* Set room flag */
	zcave[y][x].info |= (CAVE_ROOM);

	if (get_is_floor(wpos, x, y))
	{
		for (i = -1; i <= 1; i++)
		{
			for (j = -1; j <= 1; j++)
			{
				if ((x + i >= x1) && (x + i <= x2) &&
					 (y + j >= y1) && (y + j <= y2))
				{
					add_outer_wall(wpos, x + i, y + j, light, x1, y1, x2, y2);
					if (light) zcave[y][x].info |= CAVE_GLOW;
				}
			}
		}
	}

	/* Set bounding walls */
	else if (zcave[y][x].feat == FEAT_WALL_EXTRA)
	{
		zcave[y][x].feat = feat_wall_outer;
		if (light == TRUE) zcave[y][x].info |= CAVE_GLOW;
	}

	/* Set bounding walls */
	else if (zcave[y][x].feat == FEAT_PERM_OUTER)
	{
		if (light == TRUE) zcave[y][x].info |= CAVE_GLOW;
	}
}


/*
 * Hacked distance formula - gives the 'wrong' answer
 *
 * Used to build crypts
 */
int dist2(int x1, int y1, int x2, int y2,
	int h1, int h2, int h3, int h4)
{
	int dx, dy;
	dx = abs(x2 - x1);
	dy = abs(y2 - y1);

	/*
	 * Basically this works by taking the normal pythagorean formula
	 * and using an expansion to express this in a way without the
	 * square root.  This approximate formula is then perturbed to give
	 * the distorted results.  (I found this by making a mistake when I was
	 * trying to fix the circular rooms.)
	 */

	/* h1-h4 are constants that describe the metric */
	if (dx >= 2 * dy) return (dx + (dy * h1) / h2);
	if (dy >= 2 * dx) return (dy + (dx * h1) / h2);

	/* 128/181 is approx. 1/sqrt(2) */
	return (((dx + dy) * 128) / 181 +
		(dx * dx / (dy * h3) + dy * dy / (dx * h3)) * h4);
}


/*
 * Build target vault
 *
 * This is made by two concentric "crypts" with perpendicular
 * walls creating the cross-hairs.
 */
static void build_target_vault(worldpos *wpos, int x0, int y0, int xsize, int ysize)
{
	int rad, x, y;

	int h1, h2, h3, h4;

	cave_type **zcave;
//	int dun_level = getlevel(wpos);
	if(!(zcave=getcave(wpos))) return;


	/* Make a random metric */
	h1 = randint(32) - 16;
	h2 = randint(16);
	h3 = randint(32);
	h4 = randint(32) - 16;

//	if (cheat_room) msg_print("Target Vault");

	/* Work out outer radius */
	if (xsize > ysize)
	{
		rad = ysize / 2;
	}
	else
	{
		rad = xsize / 2;
	}

	/* Make floor */
	for (x = x0 - rad; x <= x0 + rad; x++)
	{
		for (y = y0 - rad; y <= y0 + rad; y++)
		{
			cave_type *c_ptr;

			/* Access the grid */
			c_ptr = &zcave[y][x];

			/* Clear room flag */
			c_ptr->info &= ~(CAVE_ROOM);

			/* Grids in vaults are required to be "icky" */
			c_ptr->info |= (CAVE_ICKY);

			/* Inside -- floor */
			if (dist2(y0, x0, y, x, h1, h2, h3, h4) <= rad - 1)
			{
				place_floor(wpos, y, x);
			}

			/* Outside -- make it granite so that arena works */
			else
			{
				c_ptr->feat = FEAT_WALL_EXTRA;
			}

			/* Proper boundary for arena */
			if (((y + rad) == y0) || ((y - rad) == y0) ||
			    ((x + rad) == x0) || ((x - rad) == x0))
			{
				cave_set_feat(wpos, y, x, feat_wall_outer);
			}
		}
	}

	/* Find visible outer walls and set to be FEAT_OUTER */
	add_outer_wall(wpos, x0, y0, FALSE, x0 - rad - 1, y0 - rad - 1,
                   x0 + rad + 1, y0 + rad + 1);

	/* Add inner wall */
	for (x = x0 - rad / 2; x <= x0 + rad / 2; x++)
	{
		for (y = y0 - rad / 2; y <= y0 + rad / 2; y++)
		{
			if (dist2(y0, x0, y, x, h1, h2, h3, h4) == rad / 2)
			{
				/* Make an internal wall */
				cave_set_feat(wpos, y, x, feat_wall_inner);
			}
		}
	}

	/* Add perpendicular walls */
	for (x = x0 - rad; x <= x0 + rad; x++)
	{
		cave_set_feat(wpos, y0, x, feat_wall_inner);
	}

	for (y = y0 - rad; y <= y0 + rad; y++)
	{
		cave_set_feat(wpos, y, x0, feat_wall_inner);
	}

	/* Make inner vault */
	for (y = y0 - 1; y <= y0 + 1; y++)
	{
		cave_set_feat(wpos, y, x0 - 1, feat_wall_inner);
		cave_set_feat(wpos, y, x0 + 1, feat_wall_inner);
	}
	for (x = x0 - 1; x <= x0 + 1; x++)
	{
		cave_set_feat(wpos, y0 - 1, x, feat_wall_inner);
		cave_set_feat(wpos, y0 + 1, x, feat_wall_inner);
	}

	place_floor(wpos, y0, x0);


	/*
	 * Add doors to vault
	 *
	 * Get two distances so can place doors relative to centre
	 */
	x = (rad - 2) / 4 + 1;
	y = rad / 2 + x;

	add_door(wpos, x0 + x, y0);
	add_door(wpos, x0 + y, y0);
	add_door(wpos, x0 - x, y0);
	add_door(wpos, x0 - y, y0);
	add_door(wpos, x0, y0 + x);
	add_door(wpos, x0, y0 + y);
	add_door(wpos, x0, y0 - x);
	add_door(wpos, x0, y0 - y);

	/* Fill with stuff - medium difficulty */
	fill_treasure(wpos, x0 - rad, x0 + rad, y0 - rad, y0 + rad, randint(3) + 3);
}


/*
 * Random vaults
 */
static void build_type11(worldpos *wpos, int by0, int bx0)
{
	int y0, x0, xsize, ysize, vtype;

	cave_type **zcave;
	int dun_level = getlevel(wpos);
	if(!(zcave=getcave(wpos))) return;

	/* Get size -- gig enough to look good, small enough to be fairly common */
	xsize = randint(22) + 22;
	ysize = randint(11) + 11;

	/* Allocate in room_map.  If will not fit, exit */
	if (!room_alloc(wpos, xsize + 1, ysize + 1, FALSE, by0, bx0, &x0, &y0)) return;

	/*
	 * Boost the rating -- Higher than lesser vaults and lower than
	 * greater vaults
	 */
//	rating += 10;

	/* (Sometimes) Cause a special feeling */
	if ((dun_level <= 50) ||
	    (randint((dun_level - 40) * (dun_level - 40) + 1) < 400))
	{
		good_item_flag = TRUE;
	}

	/* Select type of vault */
	vtype = randint(8);

	switch (vtype)
	{
		/* Build an appropriate room */
		case 1:
		{
			build_bubble_vault(wpos, x0, y0, xsize, ysize);
			break;
		}

		case 2:
		{
			build_room_vault(wpos, x0, y0, xsize, ysize);
			break;
		}

		case 3:
		{
			build_cave_vault(wpos, x0, y0, xsize, ysize);
			break;
		}

		case 4:
		{
			build_maze_vault(wpos, x0, y0, xsize, ysize);
			break;
		}

		case 5:
		{
			build_mini_c_vault(wpos, x0, y0, xsize, ysize);
			break;
		}

		case 6:
		{
			build_castle_vault(wpos, x0, y0, xsize, ysize);
			break;
		}

		case 7:
		{
			build_target_vault(wpos, x0, y0, xsize, ysize);
			break;
		}

		/* I know how to add a few more... give me some time. */

		/* Paranoia */
		default:
		{
			return;
		}
	}
}


/*
 * Crypt room generation from Z 2.5.1
 */

/*
 * Build crypt room.
 * For every grid in the possible square, check the (fake) distance.
 * If it's less than the radius, make it a room square.
 *
 * When done fill from the inside to find the walls,
 */
static void build_type12(worldpos *wpos, int by0, int bx0)
{
	int rad, x, y, x0, y0;
	int light = FALSE;
	bool emptyflag = TRUE;
	int h1, h2, h3, h4;

	cave_type **zcave;
	int dun_level = getlevel(wpos);
	if(!(zcave=getcave(wpos))) return;

	/* Make a random metric */
	h1 = randint(32) - 16;
	h2 = randint(16);
	h3 = randint(32);
	h4 = randint(32) - 16;

	/* Occasional light */
	if (randint(dun_level) <= 5) light = TRUE;

	rad = randint(9);

	/* Allocate in room_map.  If will not fit, exit */
	if (!room_alloc(wpos, rad * 2 + 3, rad * 2 + 3, FALSE, by0, bx0, &x0, &y0)) return;

	/* Make floor */
	for (x = x0 - rad; x <= x0 + rad; x++)
	{
		for (y = y0 - rad; y <= y0 + rad; y++)
		{
			/* Clear room flag */
			zcave[y][x].info &= ~(CAVE_ROOM);

			/* Inside -- floor */
			if (dist2(y0, x0, y, x, h1, h2, h3, h4) <= rad - 1)
			{
				place_floor(wpos, y, x);
			}
			else if (distance(y0, x0, y, x) < 3)
			{
				place_floor(wpos, y, x);
			}

			/* Outside -- make it granite so that arena works */
			else
			{
				cave_set_feat(wpos, y, x, feat_wall_outer);
			}

			/* Proper boundary for arena */
			if (((y + rad) == y0) || ((y - rad) == y0) ||
			    ((x + rad) == x0) || ((x - rad) == x0))
			{
				cave_set_feat(wpos, y, x, feat_wall_outer);
			}
		}
	}

	/* Find visible outer walls and set to be FEAT_OUTER */
	add_outer_wall(wpos, x0, y0, light, x0 - rad - 1, y0 - rad - 1,
		       x0 + rad + 1, y0 + rad + 1);

	/* Check to see if there is room for an inner vault */
	for (x = x0 - 2; x <= x0 + 2; x++)
	{
		for (y = y0 - 2; y <= y0 + 2; y++)
		{
			if (!get_is_floor(wpos, x, y))
			{
				/* Wall in the way */
				emptyflag = FALSE;
			}
		}
	}

	if (emptyflag && (rand_int(2) == 0))
	{
		/* Build the vault */
		build_small_room(wpos, x0, y0);

		/* Place a treasure in the vault */
		place_object(wpos, y0, x0, FALSE, FALSE, default_obj_theme);

		/* Let's guard the treasure well */
		vault_monsters(wpos, y0, x0, rand_int(2) + 3);

		/* Traps naturally */
		vault_traps(wpos, y0, x0, 4, 4, rand_int(3) + 2);
	}
}

/*
 * Maze dungeon generator
 */

/*
 * If we wasted static memory for this, it would look like:
 *
 * static char maze[(MAX_HGT / 2) + 2][(MAX_WID / 2) + 2];
 */
typedef char maze_row[(MAX_WID / 2) + 2];

void dig(maze_row *maze, int y, int x, int d)
{
	int k;
	int dy = 0, dx = 0;

	/*
	 * first, open the wall of the new cell
	 * in the direction we come from.
	 */
	switch (d)
	{
		case 0:
		{
			maze[y][x] |= 4;
			break;
		}

		case 1:
		{
			maze[y][x] |= 8;
			break;
		}

		case 2:
		{
			maze[y][x] |= 1;
			break;
		}

		case 3:
		{
			maze[y][x] |= 2;
			break;
		}
	}

	/*
	 * try to chage direction, here 50% times.
	 * with smaller values (say 25%) the maze
	 * is made of long straight corridors. with
	 * greaters values (say 75%) the maze is
	 * very "turny".
	 */
	if (rand_range(1, 100) < 50) d = rand_range(0, 3);

	for (k = 1; k <= 4; k++)
	{
		switch (d)
		{
			case 0:
			{
				dy = 0;
				dx = 1;
				break;
			}

			case 1:
			{
				dy = -1;
				dx = 0;
				break;
			}

			case 2:
			{
				dy = 0;
				dx = -1;
				break;
			}

			case 3:
			{
				dy = 1;
				dx = 0;
				break;
			}
		}

		if (maze[y + dy][x + dx] == 0)
		{
			/*
			 * now, open the wall of the new cell
			 * in the direction we go to.
			 */
			switch (d)
			{
				case 0:
				{
					maze[y][x] |= 1;
					break;
				}

				case 1:
				{
					maze[y][x] |= 2;
					break;
				}

				case 2:
				{
					maze[y][x] |= 4;
					break;
				}

				case 3:
				{
					maze[y][x] |= 8;
					break;
				}
			}

			dig(maze, y + dy, x + dx, d);
		}

		d = (d + 1) % 4;
	}
}


/* methinks it's not always necessary that the entire level is 'maze'? */
static void generate_maze(worldpos *wpos, int corridor)
{
	int i, j, d;
	int y, dy = 0;
	int x, dx = 0;
	int m_1 = 0, m_2 = 0;
	maze_row *maze;

	int cur_hgt = dun->l_ptr->hgt;
	int cur_wid = dun->l_ptr->wid;
	int div = corridor + 1;

	cave_type **zcave;
//	int dun_level = getlevel(wpos);
	if(!(zcave=getcave(wpos))) return;


	/* Allocate temporary memory */
	C_MAKE(maze, (MAX_HGT / 2) + 2, maze_row);

	/*
	 * the empty maze is:
	 *
	 * -1 -1 ... -1 -1
	 * -1  0      0 -1
	 *  .            .
	 *  .            .
	 * -1  0      0 -1
	 * -1 -1 ... -1 -1
	 *
	 *  -1 are so-called "sentinel value".
	 *   0 are empty cells.
	 *
	 *  walls are not represented, only cells.
	 *  at the end of the algorithm each cell
	 *  contains a value that is bit mask
	 *  representing surrounding walls:
	 *
	 *         bit #1
	 *
	 *        +------+
	 *        |      |
	 * bit #2 |      | bit #0
	 *        |      |
	 *        +------+
	 *
	 *         bit #3
	 *
	 * d is the direction you are digging
	 * to. d value is the bit number:
	 * d=0 --> go east
	 * d=1 --> go north
	 * etc
	 *
	 * you need only 4 bits per cell.
	 * this gives you a very compact
	 * maze representation.
	 *
	 */
	for (j = 0; j <= (cur_hgt / div) + 1; j++)
	{
		for (i = 0; i <= (cur_wid / div) + 1; i++)
		{
			maze[j][i] = -1;
		}
	}

	for (j = 1;j <= (cur_hgt / div); j++)
	{
		for (i = 1; i <= (cur_wid / div); i++)
		{
			maze[j][i] = 0;
		}
	}

	y = rand_range(1, (cur_hgt / div));
	x = rand_range(1, (cur_wid / div));
	d = rand_range(0, 3);

	dig(maze, y, x, d);

	maze[y][x] = 0;

	for (d = 0; d <= 3; d++)
	{
		switch (d)
		{
			case 0:
			{
				dy = 0;
				dx = 1;
				m_1 = 1;
				m_2 = 4;
				break;
			}

			case 1:
			{
				dy = -1;
				dx = 0;
				m_1 = 2;
				m_2 = 8;
				break;
			}

			case 2:
			{
				dy = 0;
				dx = -1;
				m_1 = 4;
				m_2 = 1;
				break;
			}

			case 3:
			{
				dy = 1;
				dx = 0;
				m_1 = 8;
				m_2 = 2;
				break;
			}
		}

		if ((maze[y + dy][x + dx] != -1) &&
		    ((maze[y + dy][x + dx] & m_2) != 0))
		{
			maze[y][x] |= m_1;
		}
	}

	/* Translate the maze bit array into a real dungeon map -- DG */
	for (j = 1; j <= (cur_hgt / div) - 2; j++)
	{
		for (i = 1; i <= (cur_wid / div) - 2; i++)
		{
			for (dx = 0; dx < corridor; dx++)
			{
				for (dy = 0; dy < corridor; dy++)
				{
					if (maze[j][i])
					{
						//				place_floor_respectedly(wpos, j * 2, i * 2);
						place_floor_respectedly(wpos, j * div + dy, i * div + dx);
					}

					if (maze[j][i] & 1)
					{
						//				place_floor_respectedly(wpos, j * 2, i * 2 + 1);
						place_floor_respectedly(wpos, j * div + dy, i * div + dx + corridor);
					}

					if(maze[j][i] & 8)
					{
						//				place_floor_respectedly(wpos, j * 2 + 1, i * 2);
						place_floor_respectedly(wpos, j * div + dy + corridor, i * div + dx);
					}
				}
			}
		}
	}

	/* Free temporary memory */
	C_FREE(maze, (MAX_HGT / 2) + 2, maze_row);
}


#if 0	// this would make a good bottleneck
/*
 * Generate a game of life level :) and make it evolve
 */
void evolve_level(worldpos *wpos, bool noise)
{
	int i, j;

	int cw = 0, cf = 0;


	/* Add a bit of noise */
	if (noise)
	{
		for (i = 1; i < cur_wid - 1; i++)
		{
			for (j = 1; j < cur_hgt - 1; j++)
			{
				if (f_info[cave[j][i].feat].flags1 & FF1_WALL) cw++;
				if (f_info[cave[j][i].feat].flags1 & FF1_FLOOR) cf++;
			}
		}

		for (i = 1; i < cur_wid - 1; i++)
		{
			for (j = 1; j < cur_hgt - 1; j++)
			{
				cave_type *c_ptr;

				/* Access the grid */
				c_ptr = &cave[j][i];

				/* Permanent features should stay */
				if (f_info[c_ptr->feat].flags1 & FF1_PERMANENT) continue;

				/* Avoid evolving grids with object or monster */
				if (c_ptr->o_idx || c_ptr->m_idx) continue;

				/* Avoid evolving player grid */
				if ((j == py) && (i == px)) continue;

				if (magik(7))
				{
					if (cw > cf)
					{
						place_floor(j, i);
					}
					else
					{
						place_filler(j, i);
					}
				}
			}
		}
	}

	for (i = 1; i < cur_wid - 1; i++)
	{
		for (j = 1; j < cur_hgt - 1; j++)
		{
			int x, y, c;
			cave_type *c_ptr;

			/* Access the grid */
			c_ptr = &cave[j][i];

			/* Permanent features should stay */
			if (f_info[c_ptr->feat].flags1 & FF1_PERMANENT) continue;

			/* Avoid evolving grids with object or monster */
			if (c_ptr->o_idx || c_ptr->m_idx) continue;

			/* Avoid evolving player grid */
			if ((j == py) && (i == px)) continue;


			/* Reset tally */
			c = 0;

			/* Count number of surrounding walls */
			for (x = i - 1; x <= i + 1; x++)
			{
				for (y = j - 1; y <= j + 1; y++)
				{
					if ((x == i) && (y == j)) continue;
					if (f_info[cave[y][x].feat].flags1 & FF1_WALL) c++;
				}
			}

			/*
			 * Changed these parameters a bit, so that it doesn't produce
			 * too open or too narrow levels -- pelpel
			 */
			/* Starved or suffocated */
			if ((c < 4) || (c >= 7))
			{
				if (f_info[c_ptr->feat].flags1 & FF1_WALL)
				{
					place_floor(j, i);
				}
			}

			/* Spawned */
			else if ((c == 4) || (c == 5))
			{
				if (!(f_info[c_ptr->feat].flags1 & FF1_WALL))
				{
					place_filler(j, i);
				}
			}
		}
	}

	/* Notice changes */
	p_ptr->update |= (PU_VIEW | PU_MONSTERS | PU_FLOW | PU_MON_LITE);
}


static void gen_life_level(worldpos *wpos)
{
	int i, j;

	for (i = 1; i < cur_wid - 1; i++)
	{
		for (j = 1; j < cur_hgt - 1; j++)
		{
			cave[j][i].info = (CAVE_ROOM | CAVE_GLOW | CAVE_MARK);
			if (magik(45)) place_floor(j, i);
			else place_filler(j, i);
		}
	}

	evolve_level(FALSE);
	evolve_level(FALSE);
	evolve_level(FALSE);
}
#endif	// 0
/* XXX XXX Here ends the big lump of ToME cake */


/* Copy (y, x) feat(usually door) to (y2,x2), and trap it if needed.
 * - Jir - */
static void duplicate_door(worldpos *wpos, int y, int x, int y2, int x2)
{
#ifdef WIDE_CORRIDORS
	int tmp;
	cave_type **zcave;
	if(!(zcave=getcave(wpos))) return;

	/* Place the same type of door */
	zcave[y2][x2].feat = zcave[y][x].feat;

	/* let's trap this too ;) */
	if ((tmp = getlevel(wpos)) <= COMFORT_PASSAGE_DEPTH ||
			rand_int(TRAPPED_DOOR_RATE) > tmp + TRAPPED_DOOR_BASE) return;
	place_trap(wpos, y2, x2, 0);
#endif
}



/*
 * Constructs a tunnel between two points
 *
 * This function must be called BEFORE any streamers are created,
 * since we use the special "granite wall" sub-types to keep track
 * of legal places for corridors to pierce rooms.
 *
 * We use "door_flag" to prevent excessive construction of doors
 * along overlapping corridors.
 *
 * We queue the tunnel grids to prevent door creation along a corridor
 * which intersects itself.
 *
 * We queue the wall piercing grids to prevent a corridor from leaving
 * a room and then coming back in through the same entrance.
 *
 * We "pierce" grids which are "outer" walls of rooms, and when we
 * do so, we change all adjacent "outer" walls of rooms into "solid"
 * walls so that no two corridors may use adjacent grids for exits.
 *
 * The "solid" wall check prevents corridors from "chopping" the
 * corners of rooms off, as well as "silly" door placement, and
 * "excessively wide" room entrances.
 *
 * Useful "feat" values:
 *   FEAT_WALL_EXTRA -- granite walls
 *   FEAT_WALL_INNER -- inner room walls
 *   FEAT_WALL_OUTER -- outer room walls
 *   FEAT_WALL_SOLID -- solid room walls
 *   FEAT_PERM_EXTRA -- shop walls (perma)
 *   FEAT_PERM_INNER -- inner room walls (perma)
 *   FEAT_PERM_OUTER -- outer room walls (perma)
 *   FEAT_PERM_SOLID -- dungeon border (perma)
 */
static void build_tunnel(struct worldpos *wpos, int row1, int col1, int row2, int col2)
{
	int			i, y, x, tmp;
	int			tmp_row, tmp_col;
	int                 row_dir, col_dir;
	int                 start_row, start_col;
	int			main_loop_count = 0;

	bool		door_flag = FALSE;
	cave_type		*c_ptr;

	cave_type **zcave;
	if(!(zcave=getcave(wpos))) return;

	/* Reset the arrays */
	dun->tunn_n = 0;
	dun->wall_n = 0;

	/* Save the starting location */
	start_row = row1;
	start_col = col1;

	/* Start out in the correct direction */
	correct_dir(&row_dir, &col_dir, row1, col1, row2, col2);

	/* Keep going until done (or bored) */
	while ((row1 != row2) || (col1 != col2))
	{
		/* Mega-Hack -- Paranoia -- prevent infinite loops */
		if (main_loop_count++ > 2000) break;

		/* Allow bends in the tunnel */
		if (rand_int(100) < DUN_TUN_CHG)
		{
			/* Acquire the correct direction */
			correct_dir(&row_dir, &col_dir, row1, col1, row2, col2);

			/* Random direction */
			if (rand_int(100) < DUN_TUN_RND)
			{
				rand_dir(&row_dir, &col_dir);
			}
		}

		/* Get the next location */
		tmp_row = row1 + row_dir;
		tmp_col = col1 + col_dir;


		/* Extremely Important -- do not leave the dungeon */
		while (!in_bounds(tmp_row, tmp_col))
		{
			/* Acquire the correct direction */
			correct_dir(&row_dir, &col_dir, row1, col1, row2, col2);

			/* Random direction */
			if (rand_int(100) < DUN_TUN_RND)
			{
				rand_dir(&row_dir, &col_dir);
			}

			/* Get the next location */
			tmp_row = row1 + row_dir;
			tmp_col = col1 + col_dir;
		}


		/* Access the location */
		c_ptr = &zcave[tmp_row][tmp_col];

		/* Avoid the edge of the dungeon */
		if (c_ptr->feat == FEAT_PERM_SOLID) continue;

		/* Avoid the edge of vaults */
		if (c_ptr->feat == FEAT_PERM_OUTER) continue;

		/* Avoid "solid" granite walls */
		if (c_ptr->feat == FEAT_WALL_SOLID) continue;

		/* Pierce "outer" walls of rooms */
		if (c_ptr->feat == FEAT_WALL_OUTER)
		{
			/* Acquire the "next" location */
			y = tmp_row + row_dir;
			x = tmp_col + col_dir;

			/* Hack -- Avoid outer/solid permanent walls */
			if (zcave[y][x].feat == FEAT_PERM_SOLID) continue;
			if (zcave[y][x].feat == FEAT_PERM_OUTER) continue;

			/* Hack -- Avoid outer/solid granite walls */
			if (zcave[y][x].feat == FEAT_WALL_OUTER) continue;
			if (zcave[y][x].feat == FEAT_WALL_SOLID) continue;

			/* Accept this location */
			row1 = tmp_row;
			col1 = tmp_col;

			/* Save the wall location */
			if (dun->wall_n < WALL_MAX)
			{
				dun->wall[dun->wall_n].y = row1;
				dun->wall[dun->wall_n].x = col1;
				dun->wall_n++;
			}

#ifdef WIDE_CORRIDORS
			/* Save the wall location */
			if (dun->wall_n < WALL_MAX)
			{
				if (zcave[row1 + col_dir][col1 + row_dir].feat != FEAT_PERM_SOLID &&
				    zcave[row1 + col_dir][col1 + row_dir].feat != FEAT_PERM_SOLID)
				{
					dun->wall[dun->wall_n].y = row1 + col_dir;
					dun->wall[dun->wall_n].x = col1 + row_dir;
					dun->wall_n++;
				}
				else
				{
					dun->wall[dun->wall_n].y = row1;
					dun->wall[dun->wall_n].x = col1;
					dun->wall_n++;
				}
			}
#endif

#ifdef WIDE_CORRIDORS
			/* Forbid re-entry near this piercing */
			for (y = row1 - 2; y <= row1 + 2; y++)
			{
				for (x = col1 - 2; x <= col1 + 2; x++)
				{
					/* Be sure we are "in bounds" */
					if (!in_bounds(y, x)) continue;

					/* Convert adjacent "outer" walls as "solid" walls */
					if (zcave[y][x].feat == FEAT_WALL_OUTER)
					{
						/* Change the wall to a "solid" wall */
						zcave[y][x].feat = FEAT_WALL_SOLID;
					}
				}
			}
#else
			/* Forbid re-entry near this piercing */
			for (y = row1 - 1; y <= row1 + 1; y++)
			{
				for (x = col1 - 1; x <= col1 + 1; x++)
				{
					/* Convert adjacent "outer" walls as "solid" walls */
					if (zcave[y][x].feat == FEAT_WALL_OUTER)
					{
						/* Change the wall to a "solid" wall */
						zcave[y][x].feat = FEAT_WALL_SOLID;
					}
				}
			}
#endif
		}

		/* Travel quickly through rooms */
		else if (c_ptr->info & CAVE_ROOM)
		{
			/* Accept the location */
			row1 = tmp_row;
			col1 = tmp_col;
		}

		/* Tunnel through all other walls */
		else if (c_ptr->feat >= FEAT_WALL_EXTRA)
		{
			/* Accept this location */
			row1 = tmp_row;
			col1 = tmp_col;

			/* Save the tunnel location */
			if (dun->tunn_n < TUNN_MAX)
			{
				dun->tunn[dun->tunn_n].y = row1;
				dun->tunn[dun->tunn_n].x = col1;
				dun->tunn_n++;
			}

#ifdef WIDE_CORRIDORS
			/* Note that this is incredibly hacked */

			/* Make sure we're in bounds */
			if (in_bounds(row1 + col_dir, col1 + row_dir))
			{
				/* New spot to be hollowed out */
				c_ptr = &zcave[row1 + col_dir][col1 + row_dir];

				/* Make sure it's a wall we want to tunnel */
				if (c_ptr->feat == FEAT_WALL_EXTRA)
				{
					/* Save the tunnel location */
					if (dun->tunn_n < TUNN_MAX)
					{
						dun->tunn[dun->tunn_n].y = row1 + col_dir;
						dun->tunn[dun->tunn_n].x = col1 + row_dir;
						dun->tunn_n++;
					}
				}
			}
#endif

			/* Allow door in next grid */
			door_flag = FALSE;
		}

		/* Handle corridor intersections or overlaps */
		else
		{
			/* Accept the location */
			row1 = tmp_row;
			col1 = tmp_col;

			/* Collect legal door locations */
			if (!door_flag)
			{
				/* Save the door location */
				if (dun->door_n < DOOR_MAX)
				{
					dun->door[dun->door_n].y = row1;
					dun->door[dun->door_n].x = col1;
					dun->door_n++;
				}

#ifdef WIDE_CORRIDORS
#if 0
				/* Save the next door location */
				if (dun->door_n < DOOR_MAX)
				{
					if (in_bounds(row1 + col_dir, col1 + row_dir))
					{
						dun->door[dun->door_n].y = row1 + col_dir;
						dun->door[dun->door_n].x = col1 + row_dir;
						dun->door_n++;
					}
					
					/* Hack -- Duplicate the previous door */
					else
					{
						dun->door[dun->door_n].y = row1;
						dun->door[dun->door_n].x = col1;
						dun->door_n++;
					}
				}
#endif	// 0
#endif

				/* No door in next grid */
				door_flag = TRUE;
			}

			/* Hack -- allow pre-emptive tunnel termination */
			if (rand_int(100) >= DUN_TUN_CON)
			{
				/* Distance between row1 and start_row */
				tmp_row = row1 - start_row;
				if (tmp_row < 0) tmp_row = (-tmp_row);

				/* Distance between col1 and start_col */
				tmp_col = col1 - start_col;
				if (tmp_col < 0) tmp_col = (-tmp_col);

				/* Terminate the tunnel */
				if ((tmp_row > 10) || (tmp_col > 10)) break;
			}
		}
	}


	/* Turn the tunnel into corridor */
	for (i = 0; i < dun->tunn_n; i++)
	{
		/* Access the grid */
		y = dun->tunn[i].y;
		x = dun->tunn[i].x;

		/* Access the grid */
		c_ptr = &zcave[y][x];

		/* Clear previous contents, add a floor */
		c_ptr->feat = FEAT_FLOOR;
	}


	/* Apply the piercings that we found */
	for (i = 0; i < dun->wall_n; i++)
	{
		int feat;

		/* Access the grid */
		y = dun->wall[i].y;
		x = dun->wall[i].x;

		/* Access the grid */
		c_ptr = &zcave[y][x];

		/* Clear previous contents, add up floor */
		c_ptr->feat = FEAT_FLOOR;

		/* Occasional doorway */
		if (rand_int(100) < DUN_TUN_PEN)
		{
			/* Place a random door */
			place_random_door(wpos, y, x);

#ifdef WIDE_CORRIDORS
			/* Remember type of door */
			feat = zcave[y][x].feat;

			/* Make sure both halves get a door */
			if (i % 2)
			{
				/* Access the grid */
				y = dun->wall[i - 1].y;
				x = dun->wall[i - 1].x;
			}
			else
			{
				/* Access the grid */
				y = dun->wall[i + 1].y;
				x = dun->wall[i + 1].x;

				/* Increment counter */
				i++;
			}

			/* Place the same type of door */
			zcave[y][x].feat = feat;

			/* let's trap this too ;) */
			if ((tmp = getlevel(wpos)) <= COMFORT_PASSAGE_DEPTH ||
					rand_int(TRAPPED_DOOR_RATE) > tmp + TRAPPED_DOOR_BASE) continue;
			place_trap(wpos, y, x, 0);
#endif
		}
	}
}




/*
 * Count the number of "corridor" grids adjacent to the given grid.
 *
 * Note -- Assumes "in_bounds(y1, x1)"
 *
 * XXX XXX This routine currently only counts actual "empty floor"
 * grids which are not in rooms.  We might want to also count stairs,
 * open doors, closed doors, etc.
 */
static int next_to_corr(struct worldpos *wpos, int y1, int x1)
{
	int i, y, x, k = 0;
	cave_type *c_ptr;
	cave_type **zcave;
	if(!(zcave=getcave(wpos))) return(FALSE);

	/* Scan adjacent grids */
	for (i = 0; i < 4; i++)
	{
		/* Extract the location */
		y = y1 + ddy_ddd[i];
		x = x1 + ddx_ddd[i];

		/* Skip non floors */
		if (!cave_floor_bold(zcave, y, x)) continue;
		/* Access the grid */
		c_ptr = &zcave[y][x];

		/* Skip non "empty floor" grids */
		if (c_ptr->feat != FEAT_FLOOR) continue;

		/* Skip grids inside rooms */
		if (c_ptr->info & CAVE_ROOM) continue;

		/* Count these grids */
		k++;
	}

	/* Return the number of corridors */
	return (k);
}


/*
 * Determine if the given location is "between" two walls,
 * and "next to" two corridor spaces.  XXX XXX XXX
 *
 * Assumes "in_bounds(y,x)"
 */
#if 0
static bool possible_doorway(struct worldpos *wpos, int y, int x)
{
        cave_type **zcave;
        if(!(zcave=getcave(wpos))) return(FALSE);

        /* Count the adjacent corridors */
        if (next_to_corr(wpos, y, x) >= 2)
        {
                /* Check Vertical */
                if ((zcave[y-1][x].feat >= FEAT_MAGMA) &&
                    (zcave[y+1][x].feat >= FEAT_MAGMA))
                {
                        return (TRUE);
                }

                /* Check Horizontal */
                if ((zcave[y][x-1].feat >= FEAT_MAGMA) &&
                    (zcave[y][x+1].feat >= FEAT_MAGMA))
                {
                        return (TRUE);
                }
        }

        /* No doorway */
        return (FALSE);
}
#else	// 0
static int possible_doorway(struct worldpos *wpos, int y, int x)
{
	cave_type **zcave;
	if(!(zcave=getcave(wpos))) return(FALSE);

	/* Count the adjacent corridors */
	if (next_to_corr(wpos, y, x) >= 2)
	{
		/* Hack -- avoid doors next to doors */
		if (is_door(zcave[y-1][x].feat) ||
			is_door(zcave[y+1][x].feat) ||
			is_door(zcave[y][x-1].feat) ||
			is_door(zcave[y][x+1].feat))
			return (-1);

		/* Check Vertical */
		if ((zcave[y-1][x].feat >= FEAT_MAGMA) &&
		    (zcave[y+1][x].feat >= FEAT_MAGMA))
		{
			return (8);
		}
#if 1
		if (in_bounds(y-2, x) &&
			(zcave[y-2][x].feat >= FEAT_MAGMA) &&
		    (zcave[y+1][x].feat >= FEAT_MAGMA))
		{
			return (1);
		}
		if (in_bounds(y+2, x) &&
			(zcave[y-1][x].feat >= FEAT_MAGMA) &&
		    (zcave[y+2][x].feat >= FEAT_MAGMA))
		{
			return (0);
		}
#endif	// 0

		/* Check Horizontal */
		if ((zcave[y][x-1].feat >= FEAT_MAGMA) &&
		    (zcave[y][x+1].feat >= FEAT_MAGMA))
		{
			return (8);
		}
#if 1
		if (in_bounds(y, x-2) &&
			(zcave[y][x-2].feat >= FEAT_MAGMA) &&
		    (zcave[y][x+1].feat >= FEAT_MAGMA))
		{
			return (3);
		}
		if (in_bounds(y, x+2) &&
			(zcave[y][x-1].feat >= FEAT_MAGMA) &&
		    (zcave[y][x+2].feat >= FEAT_MAGMA))
		{
			return (2);
		}
#endif	// 0
	}

	/* No doorway */
	return (-1);
}
#endif	// 0


#if 0
/*
 * Places door at y, x position if at least 2 walls found
 */
static void try_door(struct worldpos *wpos, int y, int x)
{
	cave_type **zcave;
	if(!(zcave=getcave(wpos))) return;

	/* Paranoia */
	if (!in_bounds(y, x)) return;

	/* Ignore walls */
	if (zcave[y][x].feat >= FEAT_MAGMA) return;

	/* Ignore room grids */
	if (zcave[y][x].info & CAVE_ROOM) return;

	/* Occasional door (if allowed) */
	if ((rand_int(100) < DUN_TUN_JCT) && possible_doorway(wpos, y, x))
	{
		/* Place a door */
		place_random_door(wpos, y, x);
	}
}
#endif	// 0

/*
 * Places doors around y, x position
 */
static void try_doors(worldpos *wpos, int y, int x)
{
	bool dir_ok[4];
	int dir_next[4];
	int i, k, n;
	int yy, xx;

	cave_type **zcave;
	if(!(zcave=getcave(wpos))) return;

	/* Paranoia */
//	if (!in_bounds(y, x)) return;

	/* Some dungeons don't have doors at all */
//	if (d_info[dungeon_type].flags1 & (DF1_NO_DOORS)) return;

	/* Reset tally */
	n = 0;

	/* Look four cardinal directions */
	for (i = 0; i < 4; i++)
	{
		/* Assume NG */
		dir_ok[i] = FALSE;

		/* Access location */
		yy = y + ddy_ddd[i];
		xx = x + ddx_ddd[i];

		/* Out of level boundary */
		if (!in_bounds(yy, xx)) continue;

		/* Ignore walls */
//		if (f_info[cave[yy][xx].feat].flags1 & (FF1_WALL)) continue;
		if (zcave[y][x].feat >= FEAT_MAGMA) continue;

		/* Ignore room grids */
//		if (cave[yy][xx].info & (CAVE_ROOM)) continue;
		if (zcave[y][x].info & CAVE_ROOM) continue;

		/* Not a doorway */
//		if (!possible_doorway(wpos, yy, xx)) continue;
		if ((dir_next[i]=possible_doorway(wpos, yy, xx)) < 0) continue;

		/* Accept the direction */
		dir_ok[i] = TRUE;

		/* Count good spots */
		n++;
	}

	/* Use the traditional method 75% of time */
	if (rand_int(100) < 75)
	{
		for (i = 0; i < 4; i++)
		{
			/* Bad locations */
			if (!dir_ok[i]) continue;

			/* Place one of various kinds of doors */
			if (rand_int(100) < DUN_TUN_JCT)
			{
				/* Access location */
				yy = y + ddy_ddd[i];
				xx = x + ddx_ddd[i];

				/* Place a door */
				place_random_door(wpos, yy, xx);

				duplicate_door(wpos, yy, xx, yy + ddy_ddd[dir_next[i]], xx + ddx_ddd[dir_next[i]]);
			}
		}
	}

	/* Use alternative method */
	else
	{
		/* A crossroad */
		if (n == 4)
		{
			/* Clear OK flags XXX */
			for (i = 0; i < 4; i++) dir_ok[i] = FALSE;

			/* Put one or two secret doors */
			dir_ok[rand_int(4)] = TRUE;
			dir_ok[rand_int(4)] = TRUE;
		}

		/* A T-shaped intersection or two possible doorways */
		else if ((n == 3) || (n == 2))
		{
			/* Pick one random location from the list */
			k = rand_int(n);

			for (i = 0; i < 4; i++)
			{
				/* Reject all but k'th OK direction */
				if (dir_ok[i] && (k-- != 0)) dir_ok[i] = FALSE;
			}
		}

		/* Place secret door(s) */
		for (i = 0; i < 4; i++)
		{
			/* Bad location */
			if (!dir_ok[i]) continue;

			/* Access location */
			yy = y + ddy_ddd[i];
			xx = x + ddx_ddd[i];

			/* Place a secret door */
			place_secret_door(wpos, yy, xx);

			duplicate_door(wpos, yy, xx, yy + ddy_ddd[dir_next[i]], xx + ddx_ddd[dir_next[i]]);
		}
	}
}




/*
 * Attempt to build a room of the given type at the given block
 *
 * Note that we restrict the number of "crowded" rooms to reduce
 * the chance of overflowing the monster list during level creation.
 */
//static bool room_build(struct worldpos *wpos, int y0, int x0, int typ)
static bool room_build(struct worldpos *wpos, int y, int x, int typ)
{
//	int y, x, y1, x1, y2, x2;


	/* Restrict level */
	if (getlevel(wpos) < room[typ].level) return (FALSE);

	/* Restrict "crowded" rooms */
	if (dun->crowded && ((typ == 5) || (typ == 6))) return (FALSE);

#if 0
	/* Extract blocks */
	y1 = y0 + room[typ].dy1;
	y2 = y0 + room[typ].dy2;
	x1 = x0 + room[typ].dx1;
	x2 = x0 + room[typ].dx2;

	/* Never run off the screen */
	if ((y1 < 0) || (y2 >= dun->row_rooms)) return (FALSE);
	if ((x1 < 0) || (x2 >= dun->col_rooms)) return (FALSE);

	/* Verify open space */
	for (y = y1; y <= y2; y++)
	{
		for (x = x1; x <= x2; x++)
		{
			if (dun->room_map[y][x]) return (FALSE);
		}
	}

	/* XXX XXX XXX It is *extremely* important that the following */
	/* calculation is *exactly* correct to prevent memory errors */

	/* Acquire the location of the room */
	y = ((y1 + y2 + 1) * BLOCK_HGT) / 2;
	x = ((x1 + x2 + 1) * BLOCK_WID) / 2;
#endif	// 0

	/* Build a room */
	switch (typ)
	{
		/* Build an appropriate room */
		case 12: build_type12(wpos, y, x); break;
		case 11: build_type11(wpos, y, x); break;
		case 10: build_type10(wpos, y, x); break;
		case  9: build_type9 (wpos, y, x); break;
		case  8: build_type8 (wpos, y, x); break;
		case  7: build_type7 (wpos, y, x); break;
		case  6: build_type6 (wpos, y, x); break;
		case  5: build_type5 (wpos, y, x); break;
		case  4: build_type4 (wpos, y, x); break;
		case  3: build_type3 (wpos, y, x); break;
		case  2: build_type2 (wpos, y, x); break;
		case  1: build_type1 (wpos, y, x); break;

		/* Paranoia */
		default: return (FALSE);
	}

#if 0
	/* Save the room location */
	if (dun->cent_n < CENT_MAX)
	{
		dun->cent[dun->cent_n].y = y;
		dun->cent[dun->cent_n].x = x;
		dun->cent_n++;
	}

	/* Reserve some blocks */
	for (y = y1; y <= y2; y++)
	{
		for (x = x1; x <= x2; x++)
		{
			dun->room_map[y][x] = TRUE;
		}
	}

	/* Count "crowded" rooms */
	if ((typ == 5) || (typ == 6)) dun->crowded = TRUE;
#endif	// 0

	/* Success */
	return (TRUE);
}


/*
 * Generate a new dungeon level
 *
 * Note that "dun_body" adds about 4000 bytes of memory to the stack.
 */
/*
 * Hrm, I know you wish to rebalance it -- but please implement
 * d_info stuffs first.. it's coded as such :)		- Jir -
 */
static void cave_gen(struct worldpos *wpos)
{
	int i, k, y, x, y1, x1, glev;

	bool destroyed = FALSE;
	bool empty_level = FALSE, dark_empty = TRUE;
	bool cavern = FALSE;
	bool maze = FALSE, permaze = FALSE, bonus = FALSE;

	dun_data dun_body;

	cave_type **zcave;
	wilderness_type *wild;
	dungeon_type	*d_ptr = getdungeon(wpos);
	u32b flags;		/* entire-dungeon flags */

	if(!(zcave=getcave(wpos))) return;
	wild=&wild_info[wpos->wy][wpos->wx];
	flags=(wpos->wz>0 ? wild->tower->flags : wild->dungeon->flags);

	if(!flags & DUNGEON_RANDOM) return;

	glev = getlevel(wpos);
	/* Global data */
	dun = &dun_body;

	dun->l_ptr = getfloor(wpos);

	if (!rand_int(SMALL_LEVEL))
	{
		dun->l_ptr->wid = MAX_WID - rand_int(MAX_WID / SCREEN_WID * 2) * (SCREEN_WID / 2);
		dun->l_ptr->hgt = MAX_HGT - rand_int(MAX_HGT / SCREEN_HGT * 2 - 1) * (SCREEN_HGT / 2);

		dun->ratio = 100 * dun->l_ptr->wid * dun->l_ptr->hgt / MAX_HGT / MAX_WID;
	}
	else
	{
		dun->l_ptr->wid = MAX_WID;
		dun->l_ptr->hgt = MAX_HGT;

		dun->ratio = 100;
	}

	dun->l_ptr->flags1 = 0;
	if (magik(NO_TELEPORT_CHANCE)) dun->l_ptr->flags1 |= LF1_NO_TELEPORT;
	if (glev < 100 && magik(NO_MAGIC_CHANCE)) dun->l_ptr->flags1 |= LF1_NO_MAGIC;
	if (magik(NO_GENO_CHANCE)) dun->l_ptr->flags1 |= LF1_NO_GENO;
	if (magik(NO_MAP_CHANCE)) dun->l_ptr->flags1 |= LF1_NOMAP;
	if (magik(NO_MAGIC_MAP_CHANCE)) dun->l_ptr->flags1 |= LF1_NO_MAGIC_MAP;
	if (magik(NO_DESTROY_CHANCE)) dun->l_ptr->flags1 |= LF1_NO_DESTROY;

	/* TODO: copy dungeon_type flags to dun_level */
	if (d_ptr && d_ptr->flags & DUNGEON_NOMAP) dun->l_ptr->flags1 |= LF1_NOMAP;
	if (d_ptr && d_ptr->flags & DUNGEON_NO_MAGIC_MAP) dun->l_ptr->flags1 |= LF1_NO_MAGIC_MAP;

	/* Hack -- NOMAP often comes with NO_MAGIC_MAP */
	if (dun->l_ptr->flags1 & LF1_NOMAP && magik(70)) dun->l_ptr->flags1 |= LF1_NO_MAGIC_MAP;

	/* Possible cavern */
//	if ((d_ptr->flags1 & DF1_CAVERN) && (rand_int(dun_level/2) > DUN_CAVERN))
//	if (rand_int(glev/2) > DUN_CAVERN && magik(DUN_CAVERN2))
	if (rand_int(glev) > DUN_CAVERN && magik(DUN_CAVERN2))
	{
		cavern = TRUE;

#if 0
		/* Make a large fractal cave in the middle of the dungeon */
		if (cheat_room)
		{
			msg_print("Cavern on level.");
		}
#endif	// 0

		build_cavern(wpos);
	}


	/* Possible "destroyed" level */
	if ((glev > 10) && (rand_int(DUN_DEST) == 0)) destroyed = TRUE;

	/* Hack -- No destroyed "quest" levels */
	if (is_quest(wpos) || (dun->l_ptr->flags1 & LF1_NO_DESTROY))
		destroyed = FALSE;

	/* Hack -- Watery caves */
	dun->watery = glev > 5 &&
		((((glev + WATERY_OFFSET) % WATERY_CYCLE) >= (WATERY_CYCLE - WATERY_RANGE))?
		magik(WATERY_BELT_CHANCE) : magik(DUN_RIVER_CHANCE - glev * DUN_RIVER_REDUCE / 100));

//	maze = (rand_int(DUN_MAZE_FACTOR) < glev - 10 && !watery) ? TRUE : FALSE;
	maze = (!cavern && rand_int(DUN_MAZE_FACTOR) < glev - 10) ? TRUE : FALSE;

	if (maze) permaze = magik(DUN_MAZE_PERMAWALL);

//	if ((d_ptr->flags1 & (DF1_EMPTY)) ||
	if (!maze && !cavern && !rand_int(EMPTY_LEVEL))
	{
		empty_level = TRUE;
		if ((randint(DARK_EMPTY)!=1 || (randint(100) > glev)))
			dark_empty = FALSE;
	}

	/* Hack -- Start with permawalls
	 * Hope run-length do a good job :) */
	for (y = 0; y < MAX_HGT; y++)
	{
		for (x = 0; x < MAX_WID; x++)
		{
			cave_type *c_ptr = &zcave[y][x];

			/* Create granite wall */
			c_ptr->feat = FEAT_PERM_SOLID;

			/* Illuminate Arena if needed */
			if (empty_level && !dark_empty) c_ptr->info |= CAVE_GLOW;
		}
	}

	/* Hack -- then curve with basic granite */
	for (y = 1; y < dun->l_ptr->hgt - 1; y++)
	{
		for (x = 1; x < dun->l_ptr->wid - 1; x++)
		{
			cave_type *c_ptr = &zcave[y][x];

			/* Create granite wall */
			c_ptr->feat = empty_level ? FEAT_FLOOR :
				(permaze ? FEAT_PERM_INNER : FEAT_WALL_EXTRA);
		}
	}

	/* Actual maximum number of rooms on this level */
/*	dun->row_rooms = MAX_HGT / BLOCK_HGT;
	dun->col_rooms = MAX_WID / BLOCK_WID;	*/

	dun->row_rooms = dun->l_ptr->hgt / BLOCK_HGT;
	dun->col_rooms = dun->l_ptr->wid / BLOCK_WID;

	/* Initialize the room table */
	for (y = 0; y < dun->row_rooms; y++)
	{
		for (x = 0; x < dun->col_rooms; x++)
		{
			dun->room_map[y][x] = FALSE;
		}
	}


	/* No "crowded" rooms yet */
	dun->crowded = FALSE;


	/* No rooms yet */
	dun->cent_n = 0;

	/* Build some rooms */
//	if (!maze)	/* let' allow it -- more fun :) */
	for (i = 0; i < DUN_ROOMS; i++)
	{
		/* Pick a block for the room */
		y = rand_int(dun->row_rooms);
		x = rand_int(dun->col_rooms);

		/* Align dungeon rooms */
		if (dungeon_align)
		{
			/* Slide some rooms right */
			if ((x % 3) == 0) x++;

			/* Slide some rooms left */
			if ((x % 3) == 2) x--;
		}

		/* Destroyed levels are boring */
		if (destroyed)
		{
			/* The deeper you are, the more cavelike the rooms are */

			/* no caves when cavern exists: they look bad */
			k = randint(100);

			if (!cavern && (k < glev))
			{
				/* Type 10 -- Fractal cave */
				if (room_build(wpos, y, x, 10)) continue;
			}
			else
			{
				/* Attempt a "trivial" room */
#if 0
				if ((d_ptr->flags1 & DF1_CIRCULAR_ROOMS) &&
						room_build(y, x, 9))
#endif	// 0
				if (magik(30) && room_build(wpos, y, x, 9))
				{
					continue;
				}
				else if (room_build(wpos, y, x, 1)) continue;
			}

#if 0
			/* Attempt a "trivial" room */
			if (room_build(wpos, y, x, 1)) continue;
#endif	// 0

			/* Never mind */
			continue;
		}

		/* Attempt an "unusual" room */
		if (rand_int(DUN_UNUSUAL) < glev)
		{
			/* Roll for room type */
			k = rand_int(100);

			/* Attempt a very unusual room */
			if (rand_int(DUN_UNUSUAL) < glev)
			{
				/* Type 8 -- Greater vault (10%) */
				if ((k < 10) && room_build(wpos, y, x, 8)) continue;

				/* Type 7 -- Lesser vault (15%) */
				if ((k < 25) && room_build(wpos, y, x, 7)) continue;

				/* Type 6 -- Monster pit (15%) */
				if ((k < 40) && room_build(wpos, y, x, 6)) continue;

				/* Type 5 -- Monster nest (10%) */
				if ((k < 50) && room_build(wpos, y, x, 5)) continue;

				/* Type 11 -- Random vault (10%) */
				if ((k < 60) && room_build(wpos, y, x, 11)) continue;
			}

			/* Type 4 -- Large room (25%) */
			if ((k < 25) && room_build(wpos, y, x, 4)) continue;

			/* Type 3 -- Cross room (20%) */
			if ((k < 45) && room_build(wpos, y, x, 3)) continue;

			/* Type 2 -- Overlapping (20%) */
			if ((k < 65) && room_build(wpos, y, x, 2)) continue;

			/* Type 10 -- Fractal cave (15%) */
			if ((k < 80) && room_build(wpos, y, x, 10)) continue;

			/* Type 9 -- Circular (10%) */
			/* Hack - build standard rectangular rooms if needed */
			if (k < 90)
			{
//				if ((d_ptr->flags1 & DF1_CIRCULAR_ROOMS) && room_build(y, x, 1)) continue;
				if (magik(70) && room_build(wpos, y, x, 1)) continue;
				else if (room_build(wpos, y, x, 9)) continue;
			}

			/* Type 12 -- Crypt (10%) */
			if ((k < 100) && room_build(wpos, y, x, 12)) continue;
		}

		/* Attempt a trivial room */
//		if (d_ptr->flags1 & DF1_CAVE)
		if (magik(50))
		{
			if (room_build(wpos, y, x, 10)) continue;
		}
		else
		{
//			if ((d_ptr->flags1 & DF1_CIRCULAR_ROOMS) && room_build(y, x, 9)) continue;
			if (magik(30) && room_build(wpos, y, x, 9)) continue;
			else if (room_build(wpos, y, x, 1)) continue;
		}
	}

#if 1
	/* Special boundary walls -- Top */
	for (x = 0; x < dun->l_ptr->wid; x++)
	{
		cave_type *c_ptr = &zcave[0][x];

		/* Clear previous contents, add "solid" perma-wall */
		c_ptr->feat = FEAT_PERM_SOLID;
	}

	/* Special boundary walls -- Bottom */
	for (x = 0; x < dun->l_ptr->wid; x++)
	{
		cave_type *c_ptr = &zcave[dun->l_ptr->hgt-1][x];

		/* Clear previous contents, add "solid" perma-wall */
		c_ptr->feat = FEAT_PERM_SOLID;
	}

	/* Special boundary walls -- Left */
	for (y = 0; y < dun->l_ptr->hgt; y++)
	{
		cave_type *c_ptr = &zcave[y][0];

		/* Clear previous contents, add "solid" perma-wall */
		c_ptr->feat = FEAT_PERM_SOLID;
	}

	/* Special boundary walls -- Right */
	for (y = 0; y < dun->l_ptr->hgt; y++)
	{
		cave_type *c_ptr = &zcave[y][dun->l_ptr->wid-1];

		/* Clear previous contents, add "solid" perma-wall */
		c_ptr->feat = FEAT_PERM_SOLID;
	}
#endif	// 0

	if (maze)
	{
		generate_maze(wpos, randint(3));
	}
	else
	{
		/* Hack -- Scramble the room order */
		for (i = 0; i < dun->cent_n; i++)
		{
			int pick1 = rand_int(dun->cent_n);
			int pick2 = rand_int(dun->cent_n);
			y1 = dun->cent[pick1].y;
			x1 = dun->cent[pick1].x;
			dun->cent[pick1].y = dun->cent[pick2].y;
			dun->cent[pick1].x = dun->cent[pick2].x;
			dun->cent[pick2].y = y1;
			dun->cent[pick2].x = x1;
		}

		/* Start with no tunnel doors */
		dun->door_n = 0;

		/* Hack -- connect the first room to the last room */
		y = dun->cent[dun->cent_n-1].y;
		x = dun->cent[dun->cent_n-1].x;

		/* Connect all the rooms together */
		for (i = 0; i < dun->cent_n; i++)
		{
			/* Connect the room to the previous room */
			build_tunnel(wpos, dun->cent[i].y, dun->cent[i].x, y, x);

			/* Remember the "previous" room */
			y = dun->cent[i].y;
			x = dun->cent[i].x;
		}

		/* Place intersection doors	 */
		for (i = 0; i < dun->door_n; i++)
		{
			/* Extract junction location */
			y = dun->door[i].y;
			x = dun->door[i].x;

			/* Try placing doors */
#if 0
			try_door(wpos, y, x - 1);
			try_door(wpos, y, x + 1);
			try_door(wpos, y - 1, x);
			try_door(wpos, y + 1, x);
#endif	// 0
			try_doors(wpos, y , x);
		}


		/* Hack -- Add some magma streamers */
		for (i = 0; i < DUN_STR_MAG; i++)
		{
			build_streamer(wpos, FEAT_MAGMA, DUN_STR_MC, FALSE);
		}

		/* Hack -- Add some quartz streamers */
		for (i = 0; i < DUN_STR_QUA; i++)
		{
			build_streamer(wpos, FEAT_QUARTZ, DUN_STR_QC, FALSE);
		}

		/* Hack -- Add some water streamers */
		if (dun->watery)
		{
			get_mon_num_hook = vault_aux_aquatic;

			/* Prepare allocation table */
			get_mon_num_prep();

			for (i = 0; i < DUN_STR_WAT; i++)
			{
				build_streamer(wpos, FEAT_DEEP_WATER, 0, TRUE);
			}

			lake_level(wpos);

			/* Remove restriction */
			get_mon_num_hook = dungeon_aux;

			/* Prepare allocation table */
			get_mon_num_prep();
		}


		/* Destroy the level if necessary */
		if (destroyed) destroy_level(wpos);
	}


	if (!(dun->l_ptr->flags1 & LF1_NO_STAIR))
	{
		/* Place 3 or 4 down stairs near some walls */
		alloc_stairs(wpos, FEAT_MORE, rand_range(3, 4) * dun->ratio / 100 + 1, 3);

		/* Place 1 or 2 up stairs near some walls */
		alloc_stairs(wpos, FEAT_LESS, rand_range(1, 2), 3);

		/* Hack -- add *more* stairs for lowbie's sake */
		if (glev <= COMFORT_PASSAGE_DEPTH)
		{
			/* Place 3 or 4 down stairs near some walls */
			alloc_stairs(wpos, FEAT_MORE, rand_range(2, 4), 3);

			/* Place 1 or 2 up stairs near some walls */
			alloc_stairs(wpos, FEAT_LESS, rand_range(3, 4), 3);
		}
	}


	/* Determine the character location */
	new_player_spot(wpos);


	/* Basic "amount" */
	k = (glev / 3);

	if (k > 10) k = 10;
	k = k * dun->ratio / 100 + 1;
	if (k < 2) k = 2;

	if (empty_level || maze)
	{
		k *= 2;
		bonus = TRUE;
	}

	/* Pick a base number of monsters */
	i = MIN_M_ALLOC_LEVEL + randint(8);
	i = i * dun->ratio / 100 + 1;

	/* Put some monsters in the dungeon */
	for (i = i + k; i > 0; i--)
	{
		(void)alloc_monster(wpos, 0, TRUE);
	}


	/* Place some traps in the dungeon */
	alloc_object(wpos, ALLOC_SET_BOTH, ALLOC_TYP_TRAP,
			randint(k * (bonus ? 3 : 1)));

	/* Put some rubble in corridors */
	alloc_object(wpos, ALLOC_SET_CORR, ALLOC_TYP_RUBBLE, randint(k));

	/* Put some objects in rooms */
	alloc_object(wpos, ALLOC_SET_ROOM, ALLOC_TYP_OBJECT, randnor(DUN_AMT_ROOM, 3) * dun->ratio / 100 + 1);

	/* Put some objects/gold in the dungeon */
	alloc_object(wpos, ALLOC_SET_BOTH, ALLOC_TYP_OBJECT, randnor(DUN_AMT_ITEM, 3) * dun->ratio / 100 + 1);
	alloc_object(wpos, ALLOC_SET_BOTH, ALLOC_TYP_GOLD, randnor(DUN_AMT_GOLD, 3) * dun->ratio / 100 + 1);

	/* Put some altars */	/* No god, no alter */
//	alloc_object(ALLOC_SET_ROOM, ALLOC_TYP_ALTAR, randnor(DUN_AMT_ALTAR, 3) * dun->ratio / 100 + 1);

	/* Put some between gates */
	alloc_object(wpos, ALLOC_SET_ROOM, ALLOC_TYP_BETWEEN, randnor(DUN_AMT_BETWEEN, 3) * dun->ratio / 100 + 1);

	/* Put some fountains */
	alloc_object(wpos, ALLOC_SET_ROOM, ALLOC_TYP_FOUNTAIN, randnor(DUN_AMT_FOUNTAIN, 3) * dun->ratio / 100 + 1);

}



/*
 * Builds a store at a given (row, column)
 *
 * Note that the solid perma-walls are at x=0/65 and y=0/21
 *
 * As of 2.7.4 (?) the stores are placed in a more "user friendly"
 * configuration, such that the four "center" buildings always
 * have at least four grids between them, to allow easy running,
 * and the store doors tend to face the middle of town.
 *
 * The stores now lie inside boxes from 3-9 and 12-18 vertically,
 * and from 7-17, 21-31, 35-45, 49-59.  Note that there are thus
 * always at least 2 open grids between any disconnected walls.
 */
/*
 * XXX hrm apartment allows wraithes to intrude..
 */
/* NOTE: This function no longer is used for normal town generation;
 * this can be used to build 'extra' towns for some purpose, though.
 */
static void build_store(struct worldpos *wpos, int n, int yy, int xx)
{
	int                 i, y, x, y0, x0, y1, x1, y2, x2, tmp;
	int size;
	cave_type		*c_ptr;
	bool flat = FALSE;
	struct c_special *cs_ptr;

	cave_type **zcave;
	if(!(zcave=getcave(wpos))) return; /*multitowns*/

	y0 = yy * 11 + 5;
	x0 = xx * 16 + 12;

	/* Determine the store boundaries */
	y1 = y0 - randint((yy == 0) ? 3 : 2);
	y2 = y0 + randint((yy == 1) ? 3 : 2);
	x1 = x0 - randint(5);
	x2 = x0 + randint(5);

	/* Hack -- make building 9's as large as possible */
	if (n == 12)
	{
		y1 = y0 - 3;
		y2 = y0 + 3;
		x1 = x0 - 5;
		x2 = x0 + 5;
	}

	/* Hack -- make building 8's larger */
	if (n == 7)
	{
		y1 = y0 - 1 - randint(2);
		y2 = y0 + 1 + randint(2);
		x1 = x0 - 3 - randint(2);
		x2 = x0 + 3 + randint(2);
	}

	/* Hack -- try 'apartment house' */
	if (n == 13 && magik(60))
	{
		y1 = y0 - 1 - randint(2);
		y2 = y0 + 1 + randint(2);
		x1 = x0 - 3 - randint(3);
		x2 = x0 + 3 + randint(2);
		if ((x2 - x1) % 2) x2--;
		if ((y2 - y1) % 2) y2--;
		if ((x2 - x1) >= 4 && (y2 - y1) >= 4 && magik(60)) flat = TRUE;
	}

	/* Build an invulnerable rectangular building */
	for (y = y1; y <= y2; y++)
	{
		for (x = x1; x <= x2; x++)
		{
			/* Get the grid */
			c_ptr = &zcave[y][x];

			/* Clear previous contents, add "basic" perma-wall */
			c_ptr->feat = (n == 13) ? FEAT_WALL_HOUSE : FEAT_PERM_EXTRA;

			/* Hack -- The buildings are illuminated and known */
			/* c_ptr->info |= (CAVE_GLOW); */
		}
	}

	/* Hack -- Make store "8" (the old home) empty */
	if (n == 7)
	{
		for (y = y1 + 1; y < y2; y++)
		{
			for (x = x1 + 1; x < x2; x++)
			{
				/* Get the grid */
				c_ptr = &zcave[y][x];

				/* Clear contents */
				c_ptr->feat = FEAT_FLOOR;

				/* Declare this to be a room */
				c_ptr->info |= (CAVE_ROOM | CAVE_GLOW | CAVE_NOPK);
			}
		}

		/* Hack -- have everyone start in the tavern */
		new_level_down_y(wpos, (y1 + y2) / 2);
		new_level_down_x(wpos, (x1 + x2) / 2);
	}

	/* Make doorways, fill with grass and trees */
	if (n == 9)
	{
		for (y = y1 + 1; y < y2; y++)
		{
			for (x = x1 + 1; x < x2; x++)
			{
				/* Get the grid */
				c_ptr = &zcave[y][x];

				/* Clear contents */
				c_ptr->feat = FEAT_GRASS;
			}
		}

		y = (y1 + y2) / 2;
		x = (x1 + x2) / 2;

		zcave[y1][x].feat = FEAT_GRASS;
		zcave[y2][x].feat = FEAT_GRASS;
		zcave[y][x1].feat = FEAT_GRASS;
		zcave[y][x2].feat = FEAT_GRASS;

		for (i = 0; i < 5; i++)
		{
			y = rand_range(y1 + 1, y2 - 1);
			x = rand_range(x1 + 1, x2 - 1);

			c_ptr = &zcave[y][x];

			c_ptr->feat = FEAT_TREES;
		}

		return;
	}
			
	/* Pond */
	if (n == 10)
	{
		for (y = y1; y <= y2; y++)
		{
			for (x = x1; x <= x2; x++)
			{
				/* Get the grid */
				c_ptr = &zcave[y][x];

				/* Fill with water */
				c_ptr->feat = FEAT_DEEP_WATER;
			}
		}

		/* Make the pond not so "square" */
		zcave[y1][x1].feat = FEAT_DIRT;
		zcave[y1][x2].feat = FEAT_DIRT;
		zcave[y2][x1].feat = FEAT_DIRT;
		zcave[y2][x2].feat = FEAT_DIRT;

		return;
	}

	/* Building with stairs */
	if (n == 11 || n == 15)
	{
		for (y = y1; y <= y2; y++)
		{
			for (x = x1; x <= x2; x++)
			{
				/* Get the grid */
				c_ptr = &zcave[y][x];

				/* Empty it */
				c_ptr->feat = FEAT_FLOOR;

				/* Put some grass */
				if (randint(100) < 50)
					c_ptr->feat = FEAT_GRASS;
				c_ptr->info |= CAVE_NOPK;
			}
		}

		x = (x1 + x2) / 2;
		y = (y1 + y2) / 2;

		/* Access the stair grid */
		c_ptr = &zcave[y][x];

		/* Clear previous contents, add down stairs */
#ifdef NEW_DUNGEON
		if(n == 11){
			c_ptr->feat = FEAT_MORE;
			new_level_up_y(wpos, y);
			new_level_rand_y(wpos, y);
			new_level_up_x(wpos, x);
			new_level_rand_x(wpos, x);
		}
		else{
			c_ptr->feat = FEAT_LESS;
			new_level_down_y(wpos, y);
			new_level_down_x(wpos, x);
		}
#else
		c_ptr->feat = FEAT_MORE;

		/* Hack -- the players start on the stairs while coming up */
		level_up_y[0] = level_rand_y[0] = y;
		level_up_x[0] = level_rand_x[0] = x;
#endif

		return;
	}

	/* Forest */
	if (n == 12)
	{
		int xc, yc, max_dis;

		/* Find the center of the forested area */
		xc = (x1 + x2) / 2;
		yc = (y1 + y2) / 2;

		/* Find the max distance from center */
		max_dis = distance(y2, x2, yc, xc);

		for (y = y1; y <= y2; y++)
		{
			for (x = x1; x <= x2; x++)
			{
				int chance;

				/* Get the grid */
				c_ptr = &zcave[y][x];

				/* Empty it */
				c_ptr->feat = FEAT_GRASS;

				/* Calculate chance of a tree */
				chance = 100 * (distance(y, x, yc, xc));
				chance /= max_dis;
				chance = 80 - chance;

				/* Put some trees */
				if (randint(100) < chance)
					c_ptr->feat = FEAT_TREES;
			}
		}

		return;
	}

	/* House */
	if (n == 13)
	{
		int price;

#ifdef USE_MANG_HOUSE
		for (y = y1 + 1; y < y2; y++)
		{
			for (x = x1 + 1; x < x2; x++)
			{
				/* Get the grid */
				c_ptr = &zcave[y][x];

				/* Fill with floor */
				c_ptr->feat = FEAT_FLOOR;

				/* Make it "icky" */
				c_ptr->info |= CAVE_ICKY;
			}
		}
#endif	// USE_MANG_HOUSE

		if (!flat)
		{
			/* Setup some "house info" */
//			price = (x2 - x1 - 1) * (y2 - y1 - 1);
			size = (x2 - x1 - 1) * (y2 - y1 - 1);
			/*price *= 20 * price; -APD- price is now proportional to size*/
			price = size * 20;
			price *= 80 + randint(40);

			/* Remember price */
			MAKE(houses[num_houses].dna, struct dna_type);
			houses[num_houses].dna->price = price;
			houses[num_houses].x=x1;
			houses[num_houses].y=y1;
			houses[num_houses].flags=HF_RECT|HF_STOCK;
			houses[num_houses].coords.rect.width=x2-x1+1;
			houses[num_houses].coords.rect.height=y2-y1+1;
#ifdef NEW_DUNGEON
			wpcopy(&houses[num_houses].wpos, wpos);
#else
			houses[num_houses].depth = 0;
#endif
		}
		/* Hack -- apartment house */
		else
		{
			int doory = 0, doorx = 0, dy, dx;
			struct c_special *cs_ptr;
			if (magik(75)) doorx = rand_int((x2 - x1 - 1) / 2);
			else doory = rand_int((y2 - y1 - 1) / 2);

			for (x = x1; x <= x2; x++)
			{
				/* Get the grid */
				c_ptr = &zcave[(y1 + y2) / 2][x];

				/* Clear previous contents, add "basic" perma-wall */
				c_ptr->feat = FEAT_PERM_EXTRA;
			}

			for (y = y1; y <= y2; y++)
			{
				/* Get the grid */
				c_ptr = &zcave[y][(x1 + x2) / 2];

				/* Clear previous contents, add "basic" perma-wall */
				c_ptr->feat = FEAT_PERM_EXTRA;
			}

			/* Setup some "house info" */
			/* XXX slightly 'bad bargain' */
//			price = (x2 - x1 - 2) * (y2 - y1 - 2) / 4;
			size = ((x2 - x1) / 2 - 1) * ((y2 - y1) / 2 - 1);
			/*price *= 20 * price; -APD- price is now proportional to size*/
			price = size * 20;
			price *= 80 + randint(40);


			for (i = 0; i < 4; i++)
			{
#if 1
				dx = (i < 2 ? x1 : x2);
				dy = ((i % 2) ? y2 : y1);
#endif	// 0
				x = ( i < 2  ? x1 : x1 + (x2 - x1) / 2);
				y = ((i % 2) ? y1 + (y2 - y1) / 2 : y1);
				c_ptr = &zcave[y][x];

				/* Remember price */
				MAKE(houses[num_houses].dna, struct dna_type);
				houses[num_houses].dna->price = price;
				houses[num_houses].x = x;
				houses[num_houses].y = y;
				houses[num_houses].flags=HF_RECT|HF_STOCK|HF_APART;
				/* was -0 */
				houses[num_houses].coords.rect.width=(x2-x1) / 2 + 1;
				houses[num_houses].coords.rect.height=(y2-y1) / 2 + 1;
#ifdef NEW_DUNGEON
				wpcopy(&houses[num_houses].wpos, wpos);
#else
				houses[num_houses].depth = 0;
#endif

				/* MEGAHACK -- add doors here and return */

#if 1	/* savedata sometimes forget about it and crashes.. */
				dx += (i < 2 ? doorx : 0 - doorx);
				dy += ((i % 2) ? 0 - doory : doory);
#endif	// 0

				c_ptr = &zcave[dy][dx];

				/* hack -- only create houses that aren't already loaded from disk */
				if ((tmp=pick_house(wpos, dy, dx)) == -1)
				{
					/* Store door location information */
					c_ptr->feat = FEAT_HOME;
					if((cs_ptr=AddCS(c_ptr, CS_DNADOOR))){
//						cs_ptr->type=CS_DNADOOR;
						cs_ptr->sc.ptr = houses[num_houses].dna;
					}
					houses[num_houses].dx=dx;
					houses[num_houses].dy=dy;
					houses[num_houses].dna->creator=0L;
					houses[num_houses].dna->owner=0L;

#ifndef USE_MANG_HOUSE
					/* This can be changed later */
					/* XXX maybe new owner will be unhappy if size>STORE_INVEN_MAX;
					 * this will be fixed when STORE_INVEN_MAX will be removed. - Jir
					 */
					size = (size >= STORE_INVEN_MAX) ? STORE_INVEN_MAX : size;
					houses[num_houses].stock_size = size;
					houses[num_houses].stock_num = 0;
					/* TODO: pre-allocate some when launching the server */
					C_MAKE(houses[num_houses].stock, size, object_type);
#endif	// USE_MANG_HOUSE

					/* One more house */
					num_houses++;
					if((house_alloc-num_houses)<32){
						GROW(houses, house_alloc, house_alloc+512, house_type);
						house_alloc+=512;
					}
				}
				else{
					KILL(houses[num_houses].dna, struct dna_type);
					c_ptr->feat=FEAT_HOME;
					if((cs_ptr=AddCS(c_ptr, CS_DNADOOR))){
//						cs_ptr->type=CS_DNADOOR;
						cs_ptr->sc.ptr=houses[tmp].dna;
					}
				}
			}

			return;
		}
	}


	/* Pick a door direction (S,N,E,W) */
	tmp = rand_int(4);

	/* Re-roll "annoying" doors */
	while (((tmp == 0) && ((yy % 2) == 1)) ||
	    ((tmp == 1) && ((yy % 2) == 0)) ||
	    ((tmp == 2) && ((xx % 2) == 1)) ||
	    ((tmp == 3) && ((xx % 2) == 0)))
	{
		/* Pick a new direction */
		tmp = rand_int(4);
	}

	/* Extract a "door location" */
	switch (tmp)
	{
		/* Bottom side */
		case 0:
		y = y2;
		x = rand_range(x1, x2);
		break;

		/* Top side */
		case 1:
		y = y1;
		x = rand_range(x1, x2);
		break;

		/* Right side */
		case 2:
		y = rand_range(y1, y2);
		x = x2;
		break;

		/* Left side */
		default:
		y = rand_range(y1, y2);
		x = x1;
		break;
	}

	/* Access the grid */
	c_ptr = &zcave[y][x];

	/* Some buildings get special doors */
	if (n == 13 && !flat)
	{
		/* hack -- only create houses that aren't already loaded from disk */
		if ((i=pick_house(wpos, y, x)) == -1)
		{
			/* Store door location information */
			c_ptr->feat = FEAT_HOME;
			if((cs_ptr=AddCS(c_ptr, CS_DNADOOR))){
//				cs_ptr->type=CS_DNADOOR;
				cs_ptr->sc.ptr = houses[num_houses].dna;
			}
			houses[num_houses].dx=x;
			houses[num_houses].dy=y;
			houses[num_houses].dna->creator=0L;
			houses[num_houses].dna->owner=0L;

#ifndef USE_MANG_HOUSE
			/* This can be changed later */
			/* XXX maybe new owner will be unhappy if size>STORE_INVEN_MAX;
			 * this will be fixed when STORE_INVEN_MAX will be removed. - Jir
			 */
			size = (size >= STORE_INVEN_MAX) ? STORE_INVEN_MAX : size;
			houses[num_houses].stock_size = size;
			houses[num_houses].stock_num = 0;
			/* TODO: pre-allocate some when launching the server */
			C_MAKE(houses[num_houses].stock, size, object_type);
#endif	// USE_MANG_HOUSE

			/* One more house */
			num_houses++;
			if((house_alloc-num_houses)<32){
				GROW(houses, house_alloc, house_alloc+512, house_type);
				house_alloc+=512;
			}
		}
		else{
			KILL(houses[num_houses].dna, struct dna_type);
			c_ptr->feat=FEAT_HOME;
			if((cs_ptr=AddCS(c_ptr, CS_DNADOOR))){
//				cs_ptr->type=CS_DNADOOR;
				cs_ptr->sc.ptr=houses[i].dna;
			}
		}
	}
	else if (n == 14) // auctionhouse
	{
		c_ptr->feat = FEAT_PERM_EXTRA; // wants to work.	
	
	}
	else
	{
		/* Clear previous contents, add a store door */
//		c_ptr->feat = FEAT_SHOP_HEAD + n;
		c_ptr->feat = FEAT_SHOP;	/* TODO: put CS_SHOP */
		c_ptr->info |= CAVE_NOPK;

		if((cs_ptr=AddCS(c_ptr, CS_SHOP))){
			cs_ptr->sc.omni = n;
		}

		for (y1 = y - 1; y1 <= y + 1; y1++)
		{
			for (x1 = x - 1; x1 <= x + 1; x1++)
			{
				/* Get the grid */
				c_ptr = &zcave[y1][x1];

				/* Declare this to be a room */
				c_ptr->info |= CAVE_ROOM | CAVE_GLOW;
				/* Illuminate the store */
//				c_ptr->info |= CAVE_GLOW;
			}
		}
	}
}

/*
 * Build a road.
 */
static void place_street(struct worldpos *wpos, int vert, int place)
{
	int y, x, y1, y2, x1, x2;
	cave_type **zcave;
	if(!(zcave=getcave(wpos))) return;

	/* Vertical streets */
	if (vert)
	{
		x = place * 32 + 20;
		x1 = x - 2;
		x2 = x + 2;

		y1 = 5;
		y2 = MAX_HGT - 5;
	}
	else
	{
		y = place * 22 + 10;
		y1 = y - 2;
		y2 = y + 2;

		x1 = 5;
		x2 = MAX_WID - 5;
	}

	for (y = y1; y <= y2; y++)
	{
		for (x = x1; x <= x2; x++)
		{
			if (zcave[y][x].feat != FEAT_FLOOR)
				zcave[y][x].feat = FEAT_GRASS;
		}
	}

	if (vert)
	{
		x1++;
		x2--;
	}
	else
	{
		y1++;
		y2--;
	}

	for (y = y1; y <= y2; y++)
	{
		for (x = x1; x <= x2; x++)
		{
			zcave[y][x].feat = FEAT_FLOOR;
		}
	}
}



/*
 * Generate the "consistent" town features, and place the player
 *
 * Hack -- play with the R.N.G. to always yield the same town
 * layout, including the size and shape of the buildings, the
 * locations of the doorways, and the location of the stairs.
 */
static void town_gen_hack(struct worldpos *wpos)
{
	int			y, x, k, n;

#ifdef	DEVEL_TOWN_COMPATIBILITY
	/* -APD- the location of the auction house */
	int			auction_x, auction_y;
#endif

	int                 rooms[72];
	/* int                 rooms[MAX_STORES]; */
	cave_type **zcave;
	if(!(zcave=getcave(wpos))) return;

	/* Hack -- Use the "simple" RNG */
	Rand_quick = TRUE;

	/* Hack -- Induce consistant town layout */
	Rand_value = seed_town+(wpos->wx+wpos->wy*MAX_WILD_X);

	/* Hack -- Start with basic floors */
	for (y = 1; y < MAX_HGT - 1; y++)
	{
		for (x = 1; x < MAX_WID - 1; x++)
		{
			cave_type *c_ptr = &zcave[y][x];

			/* Clear all features, set to "empty floor" */
			c_ptr->feat = FEAT_DIRT;

			if (rand_int(100) < 75)
			{
				c_ptr->feat = FEAT_GRASS;
			}

			else if (rand_int(100) < 15)
			{
				c_ptr->feat = FEAT_TREES;
			}
		}
	}

	/* Place horizontal "streets" */
	for (y = 0; y < 3; y++)
	{
		place_street(wpos, 0, y);
	}

	/* Place vertical "streets" */
	for (x = 0; x < 6; x++)
	{
		place_street(wpos, 1, x);
	}

#ifdef DEVEL_TOWN_COMPATIBILITY

	/* -APD- place the auction house near the central stores */

	auction_x = rand_int(5) + 3;
	if ( (auction_x == 3) || (auction_x == 8) ) auction_y = rand_int(1) + 1;
	else auction_y = (rand_int(1) * 3) + 1; // 1 or 4
#endif

	/* Prepare an Array of "remaining stores", and count them */
#ifdef NEW_DUNGEON
	for (n = 0; n < MAX_STORES-1; n++) rooms[n] = n;
	for (n = MAX_STORES-1; n < 16; n++) rooms[n] = 10;
	for (n = 16; n < 68; n++) rooms[n] = 13;
	for (n = 68; n < 72; n++) rooms[n] = 12;
	if(wild_info[wpos->wy][wpos->wx].flags & WILD_F_DOWN)
		rooms[70] = 11;
	if(wild_info[wpos->wy][wpos->wx].flags & WILD_F_UP)
		rooms[71] = 15;
#else
	for (n = 0; n < MAX_STORES-1; n++) rooms[n] = n;
	for (n = MAX_STORES-1; n < 16; n++) rooms[n] = 10;
	for (n = 16; n < 68; n++) rooms[n] = 13;
	for (n = 68; n < 71; n++) rooms[n] = 12;
	rooms[n++] = 11;
#endif

	/* Place stores */
	for (y = 2; y < 4; y++)
	{
		for (x = 4; x < 8; x++)
		{
			/* Pick a random unplaced store */
			k = rand_int(n - 64);

			/* Build that store at the proper location */
			build_store(wpos, rooms[k], y, x);

			/* One less store */
			n--;

			/* Shift the stores down, remove one store */
			rooms[k] = rooms[n - 64];
		}
	}

	/* Place two rows of stores */
	for (y = 0; y < 6; y++)
	{
		/* Place four stores per row */
		for (x = 0; x < 12; x++)
		{
			/* Make sure we haven't already placed this one */
			if (y >= 2 && y <= 3 && x >= 4 && x <= 7) continue;
			
			/* Pick a random unplaced store */
			k = rand_int(n) + 8;

#ifdef	DEVEL_TOWN_COMPATIBILITY
			if ( (y != auction_y) || (x != auction_x) ) 
			{
				/* Build that store at the proper location */
				build_store(wpos, rooms[k], y, x);
			}
			
			else /* auction time! */
			{
				build_store(wpos, 14, y, x);
			}
#else
			build_store(wpos, rooms[k], y, x);
#endif

			/* One less building */
			n--;

			/* Shift the stores down, remove one store */
			rooms[k] = rooms[n + 8];
		}
	}

	/* Hack -- use the "complex" RNG */
	Rand_quick = FALSE;
}




/*
 * Town logic flow for generation of new town
 *
 * We start with a fully wiped cave of normal floors.
 *
 * Note that town_gen_hack() plays games with the R.N.G.
 *
 * This function does NOT do anything about the owners of the stores,
 * nor the contents thereof.  It only handles the physical layout.
 *
 * We place the player on the stairs at the same time we make them.
 *
 * Hack -- since the player always leaves the dungeon by the stairs,
 * he is always placed on the stairs, even if he left the dungeon via
 * word of recall or teleport level.
 */
 
 /*
 Hack -- since boundary walls are a 'good thing' for many of the algorithms 
 used, the feature FEAT_PERM_CLEAR was created.  It is used to create an 
 invisible boundary wall for town and wilderness levels, keeping the
 algorithms happy, and the players fooled.
 
 */
 
static void town_gen(struct worldpos *wpos)
{ 
	int		y, x;
	int xstart = 0, ystart = 0;	/* dummy, for now */

	cave_type	*c_ptr;
	cave_type	**zcave;
	if(!(zcave=getcave(wpos))) return;

	/* Perma-walls -- North/South*/
	for (x = 0; x < MAX_WID; x++)
	{
		/* North wall */
		c_ptr = &zcave[0][x];

		/* Clear previous contents, add "clear" perma-wall */
		c_ptr->feat = FEAT_PERM_CLEAR;

		/* Illuminate and memorize the walls 
		c_ptr->info |= (CAVE_GLOW | CAVE_MARK);*/

		/* South wall */
		c_ptr = &zcave[MAX_HGT-1][x];

		/* Clear previous contents, add "clear" perma-wall */
		c_ptr->feat = FEAT_PERM_CLEAR;

		/* Illuminate and memorize the walls 
		c_ptr->info |= (CAVE_GLOW);*/
	}

	/* Perma-walls -- West/East */
	for (y = 0; y < MAX_HGT; y++)
	{
		/* West wall */
		c_ptr = &zcave[y][0];

		/* Clear previous contents, add "clear" perma-wall */
		c_ptr->feat = FEAT_PERM_CLEAR;

		/* Illuminate and memorize the walls
		c_ptr->info |= (CAVE_GLOW);*/

		/* East wall */
		c_ptr = &zcave[y][MAX_WID-1];

		/* Clear previous contents, add "clear" perma-wall */
		c_ptr->feat = FEAT_PERM_CLEAR;

		/* Illuminate and memorize the walls 
		c_ptr->info |= (CAVE_GLOW);*/
	}
	

	/* Hack -- Build the buildings/stairs (from memory) */
//	town_gen_hack(wpos);
//	process_dungeon_file("t_info.txt", &ystart, &xstart, cur_hgt, cur_wid, TRUE);

	/* XXX this will be changed very soon	- Jir - */
#if 1
	if(wpos->wx==cfg.town_x && wpos->wy==cfg.town_y && !wpos->wz)
	{
		/* Hack -- Use the "simple" RNG */
		Rand_quick = TRUE;

		/* Hack -- Induce consistant town layout */
		Rand_value = seed_town+(wpos->wx+wpos->wy*MAX_WILD_X);

		/* Hack -- fill with trees
		 * This should be determined by wilderness information */
		for (x = 2; x < MAX_WID - 2; x++)
		{
			for (y = 2; y < MAX_HGT - 2; y++)
			{
				/* Access the grid */
				c_ptr = &zcave[y][x];

				/* Clear previous contents, add forest */
				c_ptr->feat = magik(98) ? FEAT_TREES : FEAT_GRASS;
			}
		}

		/* XXX The space is needed to prevent players from getting
		 * stack when entering into a town from wilderness.
		 * TODO: devise a better way */
		/* Perma-walls -- North/South*/
		for (x = 1; x < MAX_WID - 1; x++)
		{
			/* North wall */
			c_ptr = &zcave[1][x];

			/* Clear previous contents, add "clear" perma-wall */
			c_ptr->feat = FEAT_GRASS;

			/* Illuminate and memorize the walls 
			   c_ptr->info |= (CAVE_GLOW | CAVE_MARK);*/

			/* South wall */
			c_ptr = &zcave[MAX_HGT-2][x];

			/* Clear previous contents, add "clear" perma-wall */
			c_ptr->feat = FEAT_GRASS;

			/* Illuminate and memorize the walls 
			   c_ptr->info |= (CAVE_GLOW);*/
		}

		/* Perma-walls -- West/East */
		for (y = 1; y < MAX_HGT - 1; y++)
		{
			/* West wall */
			c_ptr = &zcave[y][1];

			/* Clear previous contents, add "clear" perma-wall */
			c_ptr->feat = FEAT_GRASS;

			/* Illuminate and memorize the walls
			   c_ptr->info |= (CAVE_GLOW);*/

			/* East wall */
			c_ptr = &zcave[y][MAX_WID-2];

			/* Clear previous contents, add "clear" perma-wall */
			c_ptr->feat = FEAT_GRASS;

			/* Illuminate and memorize the walls 
			   c_ptr->info |= (CAVE_GLOW);*/
		}
		/* Hack -- use the "complex" RNG */
		Rand_quick = FALSE;

		process_dungeon_file("t_info.txt", wpos, &ystart, &xstart,
				MAX_HGT, MAX_WID, TRUE);
	}
	else town_gen_hack(wpos);
#else	// 0
	process_dungeon_file("t_info.txt", wpos, &ystart, &xstart,
				MAX_HGT, MAX_WID, TRUE);
#endif	// 0


	/* Day Light */
//	if ((turn % (10L * TOWN_DAWN)) < ((10L * TOWN_DAWN) / 2))
	if (IS_DAY)
	{	
		/* Lite up the town */ 
		for (y = 0; y < MAX_HGT; y++)
		{
			for (x = 0; x < MAX_WID; x++)
			{
				c_ptr = &zcave[y][x];

				 /* Perma-Lite */
				c_ptr->info |= CAVE_GLOW;
			}
		}
	}
}





/*
 * Allocate the space needed for a dungeon level
 */
void alloc_dungeon_level(struct worldpos *wpos)
{
	int i;
	wilderness_type *w_ptr=&wild_info[wpos->wy][wpos->wx];
	struct dungeon_type *d_ptr;
	cave_type **zcave;
	/* Allocate the array of rows */
	C_MAKE(zcave, MAX_HGT, cave_type *);

	/* Allocate each row */
	for (i = 0; i < MAX_HGT; i++)
	{
		/* Allocate it */
		C_MAKE(zcave[i], MAX_WID, cave_type);
	}
	if(wpos->wz){
		struct dun_level *dlp;
		d_ptr=(wpos->wz>0?w_ptr->tower:w_ptr->dungeon);
		dlp=&d_ptr->level[ABS(wpos->wz)-1];
		dlp->cave=zcave;
	}
	else w_ptr->cave=zcave;
}

/*
 * Deallocate the space needed for a dungeon level
 */
void dealloc_dungeon_level(struct worldpos *wpos)
{
	int i;
	wilderness_type *w_ptr=&wild_info[wpos->wy][wpos->wx];
	cave_type **zcave;
#if DEBUG_LEVEL > 1
	s_printf("deallocating %s\n", wpos_format(0, wpos));
#endif

	/* Delete any monsters on that level */
	/* Hack -- don't wipe wilderness monsters */
	if (wpos->wz)
	{
		wipe_m_list(wpos);

		/* Delete any objects on that level */
		/* Hack -- don't wipe wilderness objects */
		wipe_o_list(wpos);

//		wipe_t_list(wpos);
	}
	else{
		save_guildhalls(wpos);	/* has to be done here */
	}

	zcave=getcave(wpos);
	for (i = 0; i < MAX_HGT; i++)
	{
		/* Dealloc that row */
		C_FREE(zcave[i], MAX_WID, cave_type);
	}
	/* Deallocate the array of rows */
	C_FREE(zcave, MAX_HGT, cave_type *);
	if(wpos->wz){
		struct dun_level *dlp;
		struct dungeon_type *d_ptr;
		d_ptr=(wpos->wz>0?w_ptr->tower:w_ptr->dungeon);
		dlp=&d_ptr->level[ABS(wpos->wz)-1];
		dlp->cave=NULL;
	}
	else w_ptr->cave=NULL;
}

/*
 * Attempt to deallocate the floor.
 * if check failed, this heals the monsters on depth instead,
 * so that a player won't abuse scumming.
 *
 * if min_unstatic_level option is set, applicable floors will
 * always be erased.	 - Jir -
 *
 * evileye - Levels were being unstaticed crazily. Adding a
 * protection against this happening.
 */
#if 0
void dealloc_dungeon_level_maybe(struct worldpos *wpos)
{
	if ((( getlevel(wpos) < cfg.min_unstatic_level) &&
		(0 < cfg.min_unstatic_level)) ||
		(randint(1000) > cfg.anti_scum))
		dealloc_dungeon_level(wpos);
	else heal_m_list(wpos);
}
#endif	// 0

void del_dungeon(struct worldpos *wpos, bool tower){
	struct dungeon_type *d_ptr;
	int i;
	struct worldpos twpos;
	wilderness_type *wild=&wild_info[wpos->wy][wpos->wx];

	s_printf("%s at (%d,%d) attempt remove\n", (tower ? "Tower" : "Dungeon"), wpos->wx, wpos->wy);

	wpcopy(&twpos, wpos);
	d_ptr=(tower ? wild->tower : wild->dungeon);
	if(d_ptr->flags & DUNGEON_DELETED){
		s_printf("Deleted flag\n");
		for(i=0;i<d_ptr->maxdepth;i++){
			twpos.wz=(tower ? i+1 : 0-(i+1));
			if(d_ptr->level[i].ondepth) return;
			if(d_ptr->level[i].cave) dealloc_dungeon_level(&twpos);
		}
		C_KILL(d_ptr->level, d_ptr->maxdepth, struct dun_level);
#ifdef __BORLANDC__
		if (tower) KILL(wild->tower, struct dungeon_type);
		else       KILL(wild->dungeon, struct dungeon_type);
#else
		KILL((tower ? wild->tower : wild->dungeon), struct dungeon_type);
#endif
	}
	else{
		s_printf("%s at (%d,%d) restored\n", (tower ? "Tower" : "Dungeon"), wpos->wx, wpos->wy);
		/* This really should not happen, but just in case */
		/* Re allow access to the non deleted dungeon */
		wild->flags |= (tower ? WILD_F_UP : WILD_F_DOWN);
	}
	/* Release the lock */
	s_printf("%s at (%d,%d) removed\n", (tower ? "Tower" : "Dungeon"), wpos->wx, wpos->wy);
	wild->flags &= ~(tower ? WILD_F_LOCKUP : WILD_F_LOCKDOWN);
}

void remdungeon(struct worldpos *wpos, bool tower){
	struct dungeon_type *d_ptr;
	wilderness_type *wild=&wild_info[wpos->wy][wpos->wx];

	d_ptr=(tower ? wild->tower : wild->dungeon);
	if(!d_ptr) return;

	/* Lock so that dungeon cannot be overwritten while in use */
	wild->flags |= (tower ? WILD_F_LOCKUP : WILD_F_LOCKDOWN);
	/* This will prevent players entering the dungeon */
	wild->flags &= ~(tower ? WILD_F_UP : WILD_F_DOWN);
	d_ptr->flags |= DUNGEON_DELETED;
	del_dungeon(wpos, tower);	/* Hopefully first time */
}

void adddungeon(struct worldpos *wpos, int baselevel, int maxdep, int flags, char *race, char *exclude, bool tower){
	int i;
	wilderness_type *wild;
	struct dungeon_type *d_ptr;
	wild=&wild_info[wpos->wy][wpos->wx];
	if(wild->flags & (tower ? WILD_F_LOCKUP : WILD_F_LOCKDOWN)) return;
	wild->flags |= (tower ? WILD_F_UP : WILD_F_DOWN); /* no checking */
	if (tower)
		MAKE(wild->tower, struct dungeon_type);
	else
		MAKE(wild->dungeon, struct dungeon_type);
	d_ptr=(tower ? wild->tower : wild->dungeon);
	d_ptr->baselevel=baselevel;
	d_ptr->flags=flags; 
	d_ptr->maxdepth=maxdep;
	for(i=0;i<10;i++){
		d_ptr->r_char[i]='\0';
		d_ptr->nr_char[i]='\0';
	}
	if(race!=(char*)NULL){
		strcpy(d_ptr->r_char, race);
	}
	if(exclude!=(char*)NULL){
		strcpy(d_ptr->nr_char, exclude);
	}
	C_MAKE(d_ptr->level, maxdep, struct dun_level);
	for(i=0;i<maxdep;i++){
		d_ptr->level[i].ondepth=0;
	}
}

/*
 * Generates a random dungeon level			-RAK-
 *
 * Hack -- regenerate any "overflow" levels
 *
 * Hack -- allow auto-scumming via a gameplay option.
 */
 
 
void generate_cave(struct worldpos *wpos)
{
	int i, num;
	cave_type **zcave;
	zcave=getcave(wpos);

	server_dungeon = FALSE;

	/* Generate */
	for (num = 0; TRUE; num++)
	{
		bool okay = TRUE;

		cptr why = NULL;


		/* Hack -- Reset heaps */
		/*o_max = 1;
		m_max = 1;*/

		/* Start with a blank cave */
		for (i = 0; i < MAX_HGT; i++)
		{
			/* Wipe a whole row at a time */
			C_WIPE(zcave[i], MAX_WID, cave_type);
		}


		/* Mega-Hack -- no player yet */
		/*px = py = 0;*/


		/* Mega-Hack -- no panel yet */
		/*panel_row_min = 0;
		panel_row_max = 0;
		panel_col_min = 0;
		panel_col_max = 0;*/

		/* Reset the monster generation level */
		monster_level = getlevel(wpos);

		/* Reset the object generation level */
		object_level = getlevel(wpos);

		/* Build the town */
		if (istown(wpos))
		{
			/* Small town */
			/*cur_hgt = SCREEN_HGT;
			cur_wid = SCREEN_WID;*/

			/* Determine number of panels */
			/*max_panel_rows = (cur_hgt / SCREEN_HGT) * 2 - 2;
			max_panel_cols = (cur_wid / SCREEN_WID) * 2 - 2;*/

			/* Assume illegal panel */
			/*panel_row = max_panel_rows;
			panel_col = max_panel_cols;*/

			if(wpos->wx==cfg.town_x && wpos->wy==cfg.town_y && !wpos->wz){
				/* town of angband */
				adddungeon(wpos, cfg.dun_base, cfg.dun_max, DUNGEON_RANDOM, NULL, NULL, FALSE);
			}
			/* Make a town */
			town_gen(wpos);
			setup_objects();
//			setup_traps();	// no traps on town.. 
			setup_monsters();
		}

		/* Build wilderness */
		else if (!wpos->wz)
		{
			wilderness_gen(wpos);		
		}

		/* Build a real level */
		else
		{
			process_hooks(HOOK_GEN_LEVEL, "d", wpos);
			/* Big dungeon */
			/*cur_hgt = MAX_HGT;
			cur_wid = MAX_WID;*/

			/* Determine number of panels */
			/*max_panel_rows = (cur_hgt / SCREEN_HGT) * 2 - 2;
			max_panel_cols = (cur_wid / SCREEN_WID) * 2 - 2;*/

			/* Assume illegal panel */
			/*panel_row = max_panel_rows;
			panel_col = max_panel_cols;*/

			/* Make a dungeon */
			cave_gen(wpos);
		}

		/* Prevent object over-flow */
		if (o_max >= MAX_O_IDX)
		{
			/* Message */
			why = "too many objects";

			/* Message */
			okay = FALSE;
		}

		/* Prevent monster over-flow */
		if (m_max >= MAX_M_IDX)
		{
			/* Message */
			why = "too many monsters";

			/* Message */
			okay = FALSE;
		}

#if 0	// DELETEME
		/* Prevent trap over-flow */
		if (t_max >= MAX_TR_IDX)
		{
			/* Message */
			why = "too many traps";

			/* Message */
			okay = FALSE;
		}
#endif	// 0

		/* Accept */
		if (okay) break;


		/* Message */
		if (why) s_printf("Generation restarted (%s)\n", why);

		/* Wipe the objects */
		wipe_o_list(wpos);

		/* Wipe the monsters */
		wipe_m_list(wpos);

		/* Wipe the traps */
//		wipe_t_list(wpos);

		/* Compact some objects, if necessary */
		if (o_max >= MAX_O_IDX * 3 / 4)
			compact_objects(32, FALSE);

		/* Compact some monsters, if necessary */
		if (m_max >= MAX_M_IDX * 3 / 4)
			compact_monsters(32, FALSE);

#if 0	// DELETEME
		/* Compact some traps, if necessary */
		if (t_max >= MAX_TR_IDX * 3 / 4)
			compact_traps(32, FALSE);
#endif	// 0
	}


	/* Dungeon level ready */
	server_dungeon = TRUE;
}

/* $Id$ */
/* File: monster.c */

/* Purpose: misc code for monsters */

/*
 * Copyright (c) 1989 James E. Wilson, Robert A. Koeneke
 *
 * This software may be copied and distributed for educational, research, and
 * not for profit purposes provided that this copyright and statement are
 * included in all such copies.
 */

#define SERVER

// #define DEBUG1	/* for monster generation in place_monster_one */
// #define DEBUG1_IDX 1097	/* monster index to monitor at DEBUG1 (1033) */
#define DEBUG1_IDX 1098

#include "angband.h"

/*
 * Chance of cloned ego monster being the same ego type, in percent. [50]
 */
#define CLONE_EGO_CHANCE	50

/*
 * Descriptions from PernAngband.	- Jir -
 */
#define MAX_HORROR 20
#define MAX_FUNNY 22
#define MAX_COMMENT 5

int pick_randuni(int r_idx, int Level);

static cptr horror_desc[MAX_HORROR] =
{
	"abominable",
	"abysmal",
	"appalling",
	"baleful",
	"blasphemous",

	"disgusting",
	"dreadful",
	"filthy",
	"grisly",
	"hideous",

	"hellish",
	"horrible",
	"infernal",
	"loathsome",
	"nightmarish",

	"repulsive",
	"sacrilegious",
	"terrible",
	"unclean",
	"unspeakable",
};

static cptr funny_desc[MAX_FUNNY] =
{
	"silly",
	"hilarious",
	"absurd",
	"insipid",
	"ridiculous",

	"laughable",
	"ludicrous",
	"far-out",
	"groovy",
	"postmodern",

	"fantastic",
	"dadaistic",
	"cubistic",
	"cosmic",
	"awesome",

	"incomprehensible",
	"fabulous",
	"amazing",
	"incredible",
	"chaotic",

	"wild",
	"preposterous",
};

static cptr funny_comments[MAX_COMMENT] =
{
	"Wow, cosmic, man!",
	"Rad!",
	"Groovy!",
	"Cool!",
	"Far out!"
};


static monster_race* race_info_idx(int r_idx, int ego, int randuni);
int pick_ego_monster(int r_idx, int Level);


/* Monster gain a few levels ? */
int monster_check_experience(int m_idx, bool silent)
{
	int		i, try;
	u32b		levels_gained = 0, levels_gained_tmp = 0, levels_gained_melee = 0, tmp_dice, tmp;
        monster_type    *m_ptr = &m_list[m_idx];
        monster_race    *r_ptr = race_inf(m_ptr);
	int old_level = m_ptr->level;
	/* Gain levels while possible */
	while ((m_ptr->level < MONSTER_LEVEL_MAX) &&
	       (m_ptr->exp >= (MONSTER_EXP(m_ptr->level + 1))))
	{
		/* Gain a level */
		m_ptr->level++;
#if 0 /* Check the return code now for level ups */
		if(m_ptr->owner ) {
			int i;
			for(i=1; i<=NumPlayers; i++){
				if(Players[i]->id==m_ptr->owner){
					msg_print(i, "\377UYour pet looks more experienced!");
				}
			}
		}
#endif
		/* Gain hp */
		if (magik(90))
		{
#if 0
			m_ptr->maxhp += r_ptr->hside;
			m_ptr->hp += r_ptr->hside;
#endif // no thx :)
			m_ptr->maxhp += (r_ptr->hside * r_ptr->hdice) / 20;//10
			m_ptr->hp += (r_ptr->hside * r_ptr->hdice) / 20;//10
		}

		/* Gain speed */
		if (magik(50))//30
		{
			int speed = randint(2);
			m_ptr->speed += speed;
			m_ptr->mspeed += speed;

			/* fix -- never back to 0 */
			if (m_ptr->speed < speed) m_ptr->speed = 66;/* and never forward to insanity :) */
			if (m_ptr->mspeed < speed) m_ptr->mspeed = 66;
		}

		/* Gain ac */
		if (magik(30))
		{
			m_ptr->ac += (r_ptr->ac / 15)?r_ptr->ac / 15:1;
		}

		/* Gain melee power */
		/* XXX 20d1 monster can be too horrible (20d5) */
//		if (magik(50))
		if (magik(80))
		{
#if 0 /* yeah whatever.. */
			i = rand_int(4);
			try = 20;

			while ((try--) && !m_ptr->blow[i].d_dice) i = rand_int(4);

			m_ptr->blow[i].d_dice++;
#else
			levels_gained++;
#endif
		}
	}

	/* flatten the curve for very high monster levels (+27 nether realm) */
	/* +1000: (40 -> 28, 35 -> ,) 30 -> 23, 25 -> , 20 -> 16, 15 -> , 10 -> 9, 5 -> 4 */
	/* +1500: (40 -> 25, 35 -> 23,) 30 -> 20, 25 -> 18, 20 -> 15, 15 -> 12, 10 -> 8, 5 -> 4 */
	/* +2000: (40 -> 22, 35 -> ,) 30 -> 18, 25 -> , 20 -> 14, 15 -> , 10 -> 7, 5 -> 4 */
	/* +3000: (40 -> 18, 35 -> ,) 30 -> 15, 25 -> , 20 -> 12, 15 -> , 10 -> 7, 5 -> 4 */
	/* L+40 -> x25, L+35 -> x20, L+30 -> x16, L+25 -> x12, L+20 -> x9, L+15 -> x6, L+10 -> x4, L+5 -> x2 */
	if (levels_gained != 0) levels_gained = 100000 / ((100000 / levels_gained) + 1000);
	/* cap stronger for higher monsters */
//	if (levels_gained != 0) levels_gained = 100000 / ((100000 / levels_gained) + 1000 - (10000/(r_ptr->level+10)));

	/* for more accurate results, add 2 figures */
	levels_gained *= 100;
	/* for +20 levels_gained -> double the melee damage output, store the multiplier in levels_gained */
	levels_gained /= 25; /* +25 -> +1x */
	/* at least x1 */
	levels_gained += 100;

	/* very low level monsters get a boost for damage output */
//	levels_gained += 1600 / (r_ptr->level + 10);
	levels_gained += ((levels_gained - 100) * 57) / (r_ptr->level + 10);

	/* calculate square root of the factor, to apply to dice & dice sides - C. Blue */
	tmp = levels_gained;
	tmp *= 1000;
	levels_gained_tmp = 1;
	while (tmp > 1000) {
		tmp *= 1000;
		tmp /= 1320;
		levels_gained_tmp *= 1149;
		if (levels_gained_tmp >= 1000000) levels_gained_tmp /= 1000;
	}
	/* keep some figures for more accuracy (instead of / 1000) */
	levels_gained_tmp /= 100; /* now is value * 100. so we have 2 extra accuracy figures */
	
	for (i = 0; i < 4; i++) {
		/* to fix rounding issues and prevent super monsters (2d50 -> 3d50 at +1 level) */
		tmp_dice = m_ptr->blow[i].d_dice;
		if (!tmp_dice) continue;
		/* Add a limit for insanity attacks */
//disabled this, instead I just weakened Water Demons! - C. Blue -	if (m_ptr->blow[i].effect == RBE_SANITY) levels_gained_melee /= 2;
		levels_gained_melee = levels_gained_tmp;

//	    	m_ptr->blow[i].d_dice += (m_ptr->blow[i].d_dice * levels_gained) / 10;
//		m_ptr->blow[i].d_side += (m_ptr->blow[i].d_side * levels_gained) / 10;
//		m_ptr->blow[i].d_dice += levels_gained;

#if 0
		/* round dice upwards sometimes */
		if (((m_ptr->blow[i].d_dice * (levels_gained_melee - 100) / 100) * 100) <
		    (m_ptr->blow[i].d_dice * (levels_gained_melee - 100))) {
			/* Don't round up for very low monsters, or they become very hard for low players at 7 levels ood */
			m_ptr->blow[i].d_dice += (m_ptr->blow[i].d_dice * (levels_gained_melee - 100) / 100) + (r_ptr->level > 20 ? 1 : 0);
		} else {
			m_ptr->blow[i].d_dice += (m_ptr->blow[i].d_dice * (levels_gained_melee - 100) / 100);
		}
		
		/* Catch rounding problems */
		m_ptr->blow[i].d_dice = tmp_dice + (tmp_dice * (levels_gained_melee - 100) / 100) + (r_ptr->level > 20 ? 1 : 0);
		levels_gained_melee = (((m_ptr->blow[i].d_dice - tmp_dice) * 100) / tmp_dice) + 100;
		
		/* round sides upwards sometimes */
		if (((m_ptr->blow[i].d_side * (levels_gained_melee - 100) / 100) * 100) <
		    (m_ptr->blow[i].d_side * (levels_gained_melee - 100))) {
			/* Don't round up for very low monsters, or they become very hard for low players at 7 levels ood */
			m_ptr->blow[i].d_side += (m_ptr->blow[i].d_side * (levels_gained_melee - 100) / 100) + (r_ptr->level > 20 ? 1 : 0);
		} else {
			m_ptr->blow[i].d_side += (m_ptr->blow[i].d_side * (levels_gained_melee - 100) / 100);
		}
#endif
		/* round dice downwards */
		m_ptr->blow[i].d_dice += (m_ptr->blow[i].d_dice * (levels_gained_melee - 100) / 100);

		/* Catch rounding problems */
		levels_gained_melee = (((m_ptr->blow[i].d_dice - tmp_dice) * 100) / tmp_dice) + 100;
		levels_gained_melee = (levels_gained_tmp * levels_gained_tmp) / (levels_gained_melee);

		/* round sides upwards sometimes */
		if (((m_ptr->blow[i].d_side * (levels_gained_melee - 100) / 100) * 100) <
		    (m_ptr->blow[i].d_side * (levels_gained_melee - 100))) {
			/* Don't round up for very low monsters, or they become very hard for low players at 7 levels ood */
			m_ptr->blow[i].d_side += (m_ptr->blow[i].d_side * (levels_gained_melee - 100) / 100) + (r_ptr->level > 20 ? 1 : 0);
		} else {
			m_ptr->blow[i].d_side += (m_ptr->blow[i].d_side * (levels_gained_melee - 100) / 100);
		}
		
	}

	/* Insanity caps */
	for (i = 0; i < 4; i++) {
        	if ((m_ptr->blow[i].effect == RBE_SANITY) && m_ptr->blow[i].d_dice && m_ptr->blow[i].d_side) {
			while (((m_ptr->blow[i].d_dice + 1) * m_ptr->blow[i].d_side) / 2 > 55) {
				if (m_ptr->blow[i].d_dice >= m_ptr->blow[i].d_side)
					m_ptr->blow[i].d_dice--;
				else
					m_ptr->blow[i].d_side--;
			}
		}
	}

	/* Sanity caps */
	while (TRUE) {
		tmp = 0; try = 0;
		for (i = 0; i < 4; i++) {
			/* Look for attacks */
			if ((!m_ptr->blow[i].d_dice) || (!m_ptr->blow[i].d_side)) continue;
			/* Add up to total damage counter (average) */
			tmp += ((m_ptr->blow[i].d_dice + 1) * m_ptr->blow[i].d_side) / 2;
			/* Skip non-powerful attacks */
			if (((m_ptr->blow[i].d_dice + 1) * m_ptr->blow[i].d_side) / 2 <= 125) continue;
			/* Count number of powerful attacks */
			try++;
		}
		if (tmp <= AVG_MELEE_CAP) break;
		/* Randomly pick one of the attacks to lower */
		try = randint(try);
		tmp = 0;
		for (i = 0; i < 4; i++) {
			if ((!m_ptr->blow[i].d_dice) || (!m_ptr->blow[i].d_side)) continue;
			if (((m_ptr->blow[i].d_dice + 1) * m_ptr->blow[i].d_side) / 2 <= 125) continue;
			tmp++;
			if (tmp == try) {
                                /* Don't reduce by too great steps */
                                if (m_ptr->blow[i].d_dice <= 10) m_ptr->blow[i].d_side--;
                                else if (m_ptr->blow[i].d_side <= 10) m_ptr->blow[i].d_dice--;
				/* Otherwise reduce randomly */
				else if (rand_int(2)) m_ptr->blow[i].d_side--;
				else m_ptr->blow[i].d_dice--;
                        }
		}
	} 
	return (old_level-m_ptr->level == 0? 0 : 1);
}

/* Monster gain some xp */
int monster_gain_exp(int m_idx, u32b exp, bool silent)
{
	monster_type *m_ptr = &m_list[m_idx];

	m_ptr->exp += exp;

	return monster_check_experience(m_idx, silent);
}


/*
 * Delete a monster by index.
 *
 * This function causes the given monster to cease to exist for
 * all intents and purposes.  The monster record is left in place
 * but the record is wiped, marking it as "dead" (no race index)
 * so that it can be "skipped" when scanning the monster array,
 * and "excised" from the "m_fast" array when needed.
 *
 * Thus, anyone who makes direct reference to the "m_list[]" array
 * using monster indexes that may have become invalid should be sure
 * to verify that the "r_idx" field is non-zero.  All references
 * to "m_list[c_ptr->m_idx]" are guaranteed to be valid, see below.
 */
void delete_monster_idx(int i, bool unfound_arts)
{
	int x, y, Ind;
	struct worldpos *wpos;
	cave_type **zcave;
	monster_type *m_ptr = &m_list[i];
	u16b this_o_idx, next_o_idx = 0;

        monster_race *r_ptr = race_inf(m_ptr);

	/* Get location */
	y = m_ptr->fy;
	x = m_ptr->fx;
	wpos=&m_ptr->wpos;

	/* Hack -- Reduce the racial counter */
	r_ptr->cur_num--;

	/* Hack -- count the number of "reproducers" */
	if (r_ptr->flags7 & RF7_MULTIPLY) num_repro--;


	/* Remove him from everybody's view */
	for (Ind = 1; Ind < NumPlayers + 1; Ind++)
	{
		/* Skip this player if he isn't playing */
		if (Players[Ind]->conn == NOT_CONNECTED) continue;
#ifdef RPG_SERVER
		if (Players[Ind]->id == m_ptr->owner && m_ptr->pet) {
			msg_print(Ind, "\377RYour pet has died! You feel sad.");
			Players[Ind]->has_pet=0;
		}
#endif 
		Players[Ind]->mon_vis[i] = FALSE;
		Players[Ind]->mon_los[i] = FALSE;

		/* Hack -- remove target monster */
		if (i == Players[Ind]->target_who) Players[Ind]->target_who = 0;

		/* Hack -- remove tracked monster */
		if (i == Players[Ind]->health_who) health_track(Ind, 0);
	}


	/* Monster is gone */
	/* Make sure the level is allocated, it won't be if we are
	 * clearing an abandoned wilderness level of monsters
	 */
	if((zcave=getcave(wpos))){
		zcave[y][x].m_idx=0;
	}
	/* Visual update */
	everyone_lite_spot(wpos, y, x);

#ifdef MONSTER_INVENTORY
	/* Delete objects */
	for (this_o_idx = m_ptr->hold_o_idx; this_o_idx; this_o_idx = next_o_idx)
	{
		object_type *o_ptr;

		/* Acquire object */
		o_ptr = &o_list[this_o_idx];

		/* Acquire next object */
		next_o_idx = o_ptr->next_o_idx;

		/* Hack -- efficiency */
		o_ptr->held_m_idx = 0;

		/* Hack -- Preserve unknown artifacts */
/*		if (true_artifact_p(o_ptr))
		{
			handle_art_d(o_ptr->name1);
		}
*/
		/* Delete the object */
		delete_object_idx(this_o_idx, unfound_arts);
	}
#endif	// MONSTER_INVENTORY

	/* Wipe the Monster */
	FREE(m_ptr->r_ptr, monster_race);
	WIPE(m_ptr, monster_type);
}


/*
 * Delete the monster, if any, at a given location
 */
void delete_monster(struct worldpos *wpos, int y, int x, bool unfound_arts)
{
	cave_type *c_ptr;

	/* Paranoia */
	cave_type **zcave;
	if (!in_bounds(y, x)) return;
	if((zcave=getcave(wpos))){
		/* Check the grid */
		c_ptr=&zcave[y][x];
		/* Delete the monster (if any) */
		if (c_ptr->m_idx > 0) delete_monster_idx(c_ptr->m_idx, unfound_arts);
	}
	else{                           /* still delete the monster, just slower method */
		int i;
		for(i=0;i<m_max;i++){
			monster_type *m_ptr=&m_list[i];
			if(m_ptr->r_idx && inarea(wpos, &m_ptr->wpos))
			{
				if(y==m_ptr->fy && x==m_ptr->fx)
					delete_monster_idx(i, unfound_arts);
			}
		}
	}
}


/*
 * Compact and Reorder the monster list
 *
 * This function can be very dangerous, use with caution!
 *
 * When actually "compacting" monsters, we base the saving throw
 * on a combination of monster level, distance from player, and
 * current "desperation".
 *
 * After "compacting" (if needed), we "reorder" the monsters into a more
 * compact order, and we reset the allocation info, and the "live" array.
 */
/*
 * if 'purge', non-allocated wilderness monsters will be purged.
 */
void compact_monsters(int size, bool purge)
{
	int             i, num, cnt, Ind;

	int             cur_lev, cur_dis, chance;
			cave_type **zcave;
			struct worldpos *wpos;

	u16b this_o_idx, next_o_idx = 0;

	/* Message (only if compacting) */
	if (size) s_printf("Compacting monsters...\n");


	/* Compact at least 'size' objects */
	for (num = 0, cnt = 1; num < size; cnt++)
	{
		/* Get more vicious each iteration */
		cur_lev = 5 * cnt;

		/* Get closer each iteration */
		cur_dis = 5 * (20 - cnt);

		/* Check all the monsters */
		for (i = 1; i < m_max; i++)
		{
			monster_type *m_ptr = &m_list[i];

                        monster_race *r_ptr = race_inf(m_ptr);

			/* Paranoia -- skip "dead" monsters */
			if (!m_ptr->r_idx) continue;

			/* Hack -- High level monsters start out "immune" */
			if (r_ptr->level > cur_lev) continue;

			/* Ignore nearby monsters */
			if ((cur_dis > 0) && (m_ptr->cdis < cur_dis)) continue;

			/* Saving throw chance */
			chance = 90;

			/* Only compact "Quest" Monsters in emergencies */
			if ((r_ptr->flags1 & RF1_QUESTOR) && (cnt < 1000)) chance = 100;

			/* Try not to compact Unique Monsters */
			if (r_ptr->flags1 & RF1_UNIQUE) chance = 99;

			/* Monsters in town don't have much of a chance */
			if (istown(&m_ptr->wpos))
				chance = 70;

//			if (!getcave(&m_ptr->wpos)) chance = 0;

			/* All monsters get a saving throw */
			if (rand_int(100) < chance) continue;

			/* Delete the monster */
			delete_monster_idx(i, TRUE);

			/* Count the monster */
			num++;
		}
	}


	/* Excise dead monsters (backwards!) */
	for (i = m_max - 1; i >= 1; i--)
	{
		/* Get the i'th monster */
		monster_type *m_ptr = &m_list[i];

		/* Skip real monsters */
		/* real monsters in unreal location are not skipped. */
#if 0
		if (m_ptr->r_idx &&
			((!m_ptr->wpos.wz && !purge) || getcave(&m_ptr->wpos))) continue;
#else
		if (m_ptr->r_idx)
		{
			if ((!m_ptr->wpos.wz && !purge) || getcave(&m_ptr->wpos)) continue;

			/* Delete the monster */
			delete_monster_idx(i, TRUE);
		}
#endif	// 0

		/* One less monster */
		m_max--;

		/* Reorder */
		if (i != m_max)
		{
			int ny = m_list[m_max].fy;
			int nx = m_list[m_max].fx;
			wpos = &m_list[m_max].wpos;

			/* Update the cave */
			/* Hack -- make sure the level is allocated, as in the wilderness
			   it sometimes will not be */
			if((zcave=getcave(wpos)))
				zcave[ny][nx].m_idx = i;

#ifdef MONSTER_INVENTORY
			/* Repair objects being carried by monster */
#if 0
			/* No point in this... - mikaelh */
			for (this_o_idx = m_ptr->hold_o_idx; this_o_idx; this_o_idx = next_o_idx)
#else
			/* Shouldn't it be this way? - mikaelh */
			for (this_o_idx = m_list[m_max].hold_o_idx; this_o_idx; this_o_idx = next_o_idx)
#endif
			{
				object_type *o_ptr;

				/* Acquire object */
				o_ptr = &o_list[this_o_idx];

				/* Acquire next object */
				next_o_idx = o_ptr->next_o_idx;

				/* Reset monster pointer */
				o_ptr->held_m_idx = i;
			}
#endif	// MONSTER_INVENTORY

			/* Structure copy */
			m_list[i] = m_list[m_max];

			/* Copy the visibility and los flags for the players */
			for (Ind = 1; Ind < NumPlayers + 1; Ind++)
			{
				if (Players[Ind]->conn == NOT_CONNECTED) continue;

				Players[Ind]->mon_vis[i] = Players[Ind]->mon_vis[m_max];
				Players[Ind]->mon_los[i] = Players[Ind]->mon_los[m_max];

				/* Hack -- Update the target */
				if (Players[Ind]->target_who == (int)(m_max)) Players[Ind]->target_who = i;

				/* Hack -- Update the health bar */
				if (Players[Ind]->health_who == (int)(m_max)) health_track(Ind, i);
			}

			/* Wipe the hole */
			WIPE(&m_list[m_max], monster_type);
		}
	}

	/* Reset "m_nxt" */
	m_nxt = m_max;


	/* Reset "m_top" */
	m_top = 0;

	/* Collect "live" monsters */
	for (i = 0; i < m_max; i++)
	{
		/* Collect indexes */
		m_fast[m_top++] = i;
	}
}


/*
 * Delete/Remove all the monsters when the player leaves the level
 *
 * This is an efficient method of simulating multiple calls to the
 * "delete_monster()" function, with no visual effects.
 *
 * Note that this only deletes monsters that on the specified depth
 */
void wipe_m_list(struct worldpos *wpos)
{
	int i;

#if 0
	/* Delete all the monsters */
	for (i = m_max - 1; i >= 1; i--)
	{
		monster_type *m_ptr = &m_list[i];

                monster_race *r_ptr = race_inf(m_ptr);

		/* Skip dead monsters */
		if (!m_ptr->r_idx) continue;

		/* Mega-Hack -- preserve Unique's XXX XXX XXX */

		/* Hack -- Reduce the racial counter */
		r_ptr->cur_num--;

		/* Monster is gone */
		cave[m_ptr->fy][m_ptr->fx].m_idx = 0;

		/* Wipe the Monster */
		WIPE(m_ptr, monster_type);
	}

	/* Reset the monster array */
	m_nxt = m_max = 1;

	/* No live monsters */
	m_top = 0;

	/* Hack -- reset "reproducer" count */
	num_repro = 0;

	/* Hack -- no more target */
	target_who = 0;

	/* Hack -- no more tracking */
	health_track(0);
#endif

	/* Delete all the monsters */
	for (i = m_max - 1; i >= 1; i--)
	{
		monster_type *m_ptr = &m_list[i];

		if (inarea(&m_ptr->wpos,wpos)) {
#ifdef HALLOWEEN                                                                                                                                                       
                        if (m_ptr->r_idx == 1086 || m_ptr->r_idx == 1087 || m_ptr->r_idx == 1088) great_pumpkin_timer = rand_int(2); /* fast respawn if not killed! */ 
#endif                                                                                                                                                                 
			delete_monster_idx(i, TRUE);
		}
	}

	/* Compact the monster list */
	compact_monsters(0, FALSE);
}
void wipe_m_list_chance(struct worldpos *wpos, int chance)
{
	int i;

	/* Delete all the monsters, except for target dummies (1101), because those are
	   usually in town, and this function is called periodically for town! */
	for (i = m_max - 1; i >= 1; i--)
	{
		monster_type *m_ptr = &m_list[i];

		if (inarea(&m_ptr->wpos,wpos) && magik(chance) && (m_ptr->r_idx != 1101) && (m_ptr->r_idx != 1102)) { /* hardcoded -_- */
#ifdef HALLOWEEN
			if (m_ptr->r_idx == 1086 || m_ptr->r_idx == 1087 || m_ptr->r_idx == 1088) great_pumpkin_timer = rand_int(2); /* fast respawn if not killed! */
#endif	
			delete_monster_idx(i, TRUE);
		}
	}

	/* Compact the monster list */
	compact_monsters(0, FALSE);
}
/* only wipes monsters not on CAVE_ICKY (for pvp arena) */
void wipe_m_list_roaming(struct worldpos *wpos)
{
	int i;
	cave_type **zcave;
	if(!(zcave=getcave(wpos))) return;
	for (i = m_max - 1; i >= 1; i--) {
		monster_type *m_ptr = &m_list[i];
		if (inarea(&m_ptr->wpos,wpos)) {
			if (zcave[m_ptr->fy][m_ptr->fx].info & CAVE_ICKY) continue;
			delete_monster_idx(i, TRUE);
		}
	}
	/* Compact the monster list */
	compact_monsters(0, FALSE);
}

/* 
 * Heal up every monster on the depth, so that a player
 * cannot abuse stair-GoI and anti-scum.	- Jir -
 */
#if 0
void heal_m_list(struct worldpos *wpos)
{
	int i;

	/* Heal all the monsters */
	for (i = m_max - 1; i >= 1; i--)
	{
		monster_type *m_ptr = &m_list[i];

		if (inarea(&m_ptr->wpos,wpos))
			m_ptr->hp = m_ptr->maxhp;
//			delete_monster_idx(i);
	}

	/* Compact the monster list */
//	compact_monsters(0);
}
#endif	// 0


/*
 * Acquires and returns the index of a "free" monster.
 *
 * This routine should almost never fail, but it *can* happen.
 *
 * Note that this function must maintain the special "m_fast"
 * array of indexes of "live" monsters.
 */
s16b m_pop(void)
{
	int i, n, k;


	/* Normal allocation */
	if (m_max < MAX_M_IDX)
	{
		/* Access the next hole */
		i = m_max;

		/* Expand the array */
		m_max++;

		/* Update "m_fast" */
		m_fast[m_top++] = i;

		/* Return the index */
		return (i);
	}


	/* Check for some space */
	for (n = 1; n < MAX_M_IDX; n++)
	{
		/* Get next space */
		i = m_nxt;

		/* Advance (and wrap) the "next" pointer */
		if (++m_nxt >= MAX_M_IDX) m_nxt = 1;

		/* Skip monsters in use */
		if (m_list[i].r_idx) continue;

		/* Verify space XXX XXX */
		if (m_top >= MAX_M_IDX) continue;

		/* Verify not allocated */
		for (k = 0; k < m_top; k++)
		{
			/* Hack -- Prevent errors */
			if (m_fast[k] == i) i = 0;
		}

		/* Oops XXX XXX */
		if (!i) continue;

		/* Update "m_fast" */
		m_fast[m_top++] = i;

		/* Use this monster */
		return (i);
	}


	/* Warn the player */
	if (server_dungeon) s_printf("Too many monsters!");

	/* Try not to crash */
	return (0);
}




/*
 * Apply a "monster restriction function" to the "monster allocation table"
 */
errr get_mon_num_prep(void)
{
	int i;

	/* Scan the allocation table */
	for (i = 0; i < alloc_race_size; i++)
	{
		/* Get the entry */
		alloc_entry *entry = &alloc_race_table[i];

		/* Accept monsters which pass the restriction, if any */
		if ((!get_mon_num_hook || (*get_mon_num_hook)(entry->index)) &&
			(!get_mon_num2_hook || (*get_mon_num2_hook)(entry->index)))
		{
			/* Accept this monster */
			entry->prob2 = entry->prob1;
		}

		/* Do not use this monster */
		else
		{
			/* Decline this monster */
			entry->prob2 = 0;
		}
	}

	/* Success */
	return (0);
}

/* TODO: do this job when creating allocation table, for efficiency */
/* XXX: this function can act strange when used for non-generation checks */
bool mon_allowed(monster_race *r_ptr)
{
	int i = randint(100);

	/* Pet/neutral monsters allowed? */
	if(i > cfg.pet_monsters && (r_ptr->flags7 & (RF7_PET | RF7_NEUTRAL)))
				return(FALSE);

	/* C. Blue's monsters allowed ? or not ? */
	if(i > cfg.cblue_monsters && (r_ptr->flags8 & RF8_BLUEBAND)) return(FALSE);

	/* Joke monsters allowed ? or not ? */
	if(i > cfg.joke_monsters && (r_ptr->flags8 & RF8_JOKEANGBAND)) return(FALSE);

	/* Lovercraftian monsters allowed ? or not ? */
	if(i > cfg.cth_monsters && (r_ptr->flags8 & RF8_CTHANGBAND)) return(FALSE);

	/* Zangbandish monsters allowed ? or not ? */
	if(i > cfg.zang_monsters && (r_ptr->flags8 & RF8_ZANGBAND)) return(FALSE);

	/* Pernian monsters allowed ? or not ? */
	if(i > cfg.pern_monsters && (r_ptr->flags8 & RF8_PERNANGBAND)) return(FALSE);

	/* Base monsters allowed ? or not ? */
	if(i > cfg.vanilla_monsters && (r_ptr->flags8 & RF8_ANGBAND)) return(FALSE);

	return(TRUE);
}
/* added this one for monster_level_min implementation - C. Blue */
bool mon_allowed_chance(monster_race *r_ptr) {
	/* Pet/neutral monsters allowed? */
	if(r_ptr->flags7 & (RF7_PET | RF7_NEUTRAL)) return(cfg.pet_monsters);

	/* C. Blue's monsters allowed ? or not ? */
	if(r_ptr->flags8 & RF8_BLUEBAND) return(cfg.cblue_monsters);
	/* Joke monsters allowed ? or not ? */
	if (r_ptr->flags8 & RF8_JOKEANGBAND) return(cfg.joke_monsters);

	/* Lovercraftian monsters allowed ? or not ? */
	if(r_ptr->flags8 & RF8_CTHANGBAND) return(cfg.cth_monsters);

	/* Zangbandish monsters allowed ? or not ? */
	if(r_ptr->flags8 & RF8_ZANGBAND) return(cfg.zang_monsters);

	/* Pernian monsters allowed ? or not ? */
	if(r_ptr->flags8 & RF8_PERNANGBAND) return(cfg.pern_monsters);

	/* Base monsters allowed ? or not ? */
	if (r_ptr->flags8 & RF8_ANGBAND) return(cfg.vanilla_monsters);

	return(100); /* or 0? =p */
}

/* Similar to mon_allowed, but determine the result by TELL_MONSTER_ABOVE. */
bool mon_allowed_view(monster_race *r_ptr)
{
	int i = TELL_MONSTER_ABOVE;

	/* Pet/neutral monsters allowed? */
	if(i > cfg.pet_monsters && (r_ptr->flags7 & (RF7_PET | RF7_NEUTRAL)))
				return(FALSE);

	/* C. Blue's monsters allowed ? or not ? */
	if(i > cfg.cblue_monsters && (r_ptr->flags8 & RF8_BLUEBAND)) return(FALSE);

	/* Joke monsters allowed ? or not ? */
	if(i > cfg.joke_monsters && (r_ptr->flags8 & RF8_JOKEANGBAND)) return(FALSE);

	/* Lovercraftian monsters allowed ? or not ? */
	if(i > cfg.cth_monsters && (r_ptr->flags8 & RF8_CTHANGBAND)) return(FALSE);

	/* Zangbandish monsters allowed ? or not ? */
	if(i > cfg.zang_monsters && (r_ptr->flags8 & RF8_ZANGBAND)) return(FALSE);

	/* Pernian monsters allowed ? or not ? */
	if(i > cfg.pern_monsters && (r_ptr->flags8 & RF8_PERNANGBAND)) return(FALSE);

	/* Base monsters allowed ? or not ? */
	if(i > cfg.vanilla_monsters && (r_ptr->flags8 & RF8_ANGBAND)) return(FALSE);

	return(TRUE);
}


/*
 * Some dungeon types restrict the possible monsters.
 * Return TRUE is the monster is OK and FALSE otherwise
 */
//bool apply_rule(monster_race *r_ptr, byte rule)
static bool apply_rule(monster_race *r_ptr, byte rule, int dun_type)
{
	dungeon_info_type *d_ptr = &d_info[dun_type];

	if (d_ptr->rules[rule].mode == DUNGEON_MODE_NONE)
	{
		return TRUE;
	}
	else if ((d_ptr->rules[rule].mode == DUNGEON_MODE_AND) || (d_ptr->rules[rule].mode == DUNGEON_MODE_NAND))
	{
		int a;

		if (d_ptr->rules[rule].mflags1)
		{
			if((d_ptr->rules[rule].mflags1 & r_ptr->flags1) != d_ptr->rules[rule].mflags1)
				return FALSE;
		}
		if (d_ptr->rules[rule].mflags2)
		{
			if((d_ptr->rules[rule].mflags2 & r_ptr->flags2) != d_ptr->rules[rule].mflags2)
				return FALSE;
		}
		if (d_ptr->rules[rule].mflags3)
		{
			if((d_ptr->rules[rule].mflags3 & r_ptr->flags3) != d_ptr->rules[rule].mflags3)
				return FALSE;
		}
		if (d_ptr->rules[rule].mflags4)
		{
			if((d_ptr->rules[rule].mflags4 & r_ptr->flags4) != d_ptr->rules[rule].mflags4)
				return FALSE;
		}
		if (d_ptr->rules[rule].mflags5)
		{
			if((d_ptr->rules[rule].mflags5 & r_ptr->flags5) != d_ptr->rules[rule].mflags5)
				return FALSE;
		}
		if (d_ptr->rules[rule].mflags6)
		{
			if((d_ptr->rules[rule].mflags6 & r_ptr->flags6) != d_ptr->rules[rule].mflags6)
				return FALSE;
		}
		if (d_ptr->rules[rule].mflags7)
		{
			if((d_ptr->rules[rule].mflags7 & r_ptr->flags7) != d_ptr->rules[rule].mflags7)
				return FALSE;
		}
		if (d_ptr->rules[rule].mflags8)
		{
			if((d_ptr->rules[rule].mflags8 & r_ptr->flags8) != d_ptr->rules[rule].mflags8)
				return FALSE;
		}
		if (d_ptr->rules[rule].mflags9)
		{
			if((d_ptr->rules[rule].mflags9 & r_ptr->flags9) != d_ptr->rules[rule].mflags9)
				return FALSE;
		}
		for(a = 0; a < 5; a++)
		{
			if (d_ptr->rules[rule].r_char[a] && (d_ptr->rules[rule].r_char[a] != r_ptr->d_char)) return FALSE;
		}

		/* All checks passed ? lets go ! */
		return TRUE;
	}
	else if ((d_ptr->rules[rule].mode == DUNGEON_MODE_OR) || (d_ptr->rules[rule].mode == DUNGEON_MODE_NOR))
	{
		int a;

		if (d_ptr->rules[rule].mflags1 && (r_ptr->flags1 & d_ptr->rules[rule].mflags1)) return TRUE;
		if (d_ptr->rules[rule].mflags2 && (r_ptr->flags2 & d_ptr->rules[rule].mflags2)) return TRUE;
		if (d_ptr->rules[rule].mflags3 && (r_ptr->flags3 & d_ptr->rules[rule].mflags3)) return TRUE;
		if (d_ptr->rules[rule].mflags4 && (r_ptr->flags4 & d_ptr->rules[rule].mflags4)) return TRUE;
		if (d_ptr->rules[rule].mflags5 && (r_ptr->flags5 & d_ptr->rules[rule].mflags5)) return TRUE;
		if (d_ptr->rules[rule].mflags6 && (r_ptr->flags6 & d_ptr->rules[rule].mflags6)) return TRUE;
		if (d_ptr->rules[rule].mflags7 && (r_ptr->flags7 & d_ptr->rules[rule].mflags7)) return TRUE;
		if (d_ptr->rules[rule].mflags8 && (r_ptr->flags8 & d_ptr->rules[rule].mflags8)) return TRUE;
		if (d_ptr->rules[rule].mflags9 && (r_ptr->flags9 & d_ptr->rules[rule].mflags9)) return TRUE;

		for (a = 0; a < 5; a++)
			if (d_ptr->rules[rule].r_char[a] == r_ptr->d_char) return TRUE;

		/* All checks failled ? Sorry ... */
		return FALSE;
	}

	/* Should NEVER happen */
	return FALSE;
}

//bool restrict_monster_to_dungeon(int r_idx)
static bool restrict_monster_to_dungeon(int r_idx, int dun_type)
{
	dungeon_info_type *d_ptr = &d_info[dun_type];
	monster_race *r_ptr = &r_info[r_idx];

	/* Select a random rule */
	byte rule = d_ptr->rule_percents[rand_int(100)];

	/* Apply the rule */
	bool rule_ret = apply_rule(r_ptr, rule, dun_type);

	/* Should the rule be right or not ? */
	if ((d_ptr->rules[rule].mode == DUNGEON_MODE_NAND) || (d_ptr->rules[rule].mode == DUNGEON_MODE_NOR)) rule_ret = !rule_ret;

	/* Rule ok ? */
	if (rule_ret) return TRUE;

	/* Not allowed */
	return FALSE;
}


/*
 * Choose a monster race that seems "appropriate" to the given level
 *
 * This function uses the "prob2" field of the "monster allocation table",
 * and various local information, to calculate the "prob3" field of the
 * same table, which is then used to choose an "appropriate" monster, in
 * a relatively efficient manner.
 *
 * Note that "town" monsters will *only* be created in the town, and
 * "normal" monsters will *never* be created in the town, unless the
 * "level" is "modified", for example, by polymorph or summoning.
 *
 * There is a small chance (1/50) of "boosting" the given depth by
 * a small amount (up to four levels), except in the town.
 *
 * It is (slightly) more likely to acquire a monster of the given level
 * than one of a lower level.  This is done by choosing several monsters
 * appropriate to the given level and keeping the "hardest" one.
 *
 * Note that if no monsters are "appropriate", then this function will
 * fail, and return zero, but this should *almost* never happen.
 */

/* APD I am changing this to support "negative" levels, so we can have
 * wilderness only monsters.
 *
 * XXX I am temporarily disabling this, as it appears to be messing
 * things up.
 *
 * This function is kind of a mess right now.
 *
 * C. Blue: Added check of 'monster_level_min', for
 *          micro cell vaults (cross-shaped) actually:
 *    -1 = auto, 0 = disabled, >0 = use specific value.
 */

//s16b get_mon_num(int level)
s16b get_mon_num(int level, int dun_type)
{
	int                     i, j, p; //, d1 = 0, d2 = 0;
	int                     r_idx;
	long            value, total;
	monster_race    *r_ptr;
	alloc_entry             *table = alloc_race_table;
	int hack_prob = 100;

#if 0	// removed, but not disabled.. please see place_monster_one
	/* Warp level around */
	if (level > 100)
	{
		level = level - 100;
	}
#endif	// 0

	/* Out Of Depth modification: */
	if (level > 0)
	{
//commented this one out, it's too much (destroyer on lv 54..)
#if 0
		/* Occasional "nasty" monster */
		if (rand_int(NASTY_MON) == 0)
		{
			/* Pick a level bonus */
			d1 = level / 4 + 2;

			/* Boost the level */
			level += ((d1 < 5) ? d1 : 5);
		}

		/* Occasional "nasty" monster */
		if (rand_int(NASTY_MON) == 0)
		{
			/* Pick a level bonus */
			d2 = level / 4 + 2;

			/* Boost the level */
			level += ((d2 < 5) ? d2 : 5);
		}
#else
		if (rand_int(NASTY_MON) == 0)
		{
			if (level < 15) level += (2 + level/2 + randint(3));
			else level += (10 + level/4 + randint(level / 4)); /* can be quite nasty, but serves well so far >;-) */
		}
#endif
	}       

	/* Boost the level -- but not in town square. 
	if (level)
	{
		level += ((d1 < 5) ? d1 : 5);
		level += ((d2 < 5) ? d2 : 5);
	} */

	/* Reset total */
	total = 0L;

	/* Process probabilities */
	for (i = 0; i < alloc_race_size; i++)
	{
		/* Monsters are sorted by depth */
		if (table[i].level > level) break;


		/* XXX disabling this 
		{
			 ceiling 
			if (table[i].level > -level) break;
			 floor 
			if (table[i].level < level) continue; 
		}
		*/

		/* Default */
		table[i].prob3 = 0;

		/* Hack -- No town monsters in the dungeon */
		if ((level > 0) && (table[i].level <= 0)) continue;

		/* Access the "r_idx" of the chosen monster */
		r_idx = table[i].index;

		/* Access the actual race */
		r_ptr = &r_info[r_idx];

		/* Hack -- "unique" monsters must be "unique" */
		/* For each players -- DG */
/*              if ((r_ptr->flags1 & RF1_UNIQUE) &&
		    (unique_allowed(r_idx, level))
		{
			continue;
		}*/

		/* Depth Monsters never appear out of depth */
		/* FIXME: This might cause FORCE_DEPTH monsters to appear out of depth */
		if ((r_ptr->flags1 & RF1_FORCE_DEPTH) && (r_ptr->level > level))
		{
			continue;
		}

		/* Depth Monsters never appear out of their depth */
		/* level = base_depth + depth, be careful(esp.wilderness) */
		/* Seemingly it's only for Moldoux */
		if ((r_ptr->flags9 & (RF9_ONLY_DEPTH)) && (r_ptr->level != level))
		{
			continue;
		}

#if 0
/* temporarily disabled until empty spots bug has been figured out,
   probably happening because of mon_allowed checks for _BAND flavour probabilities */
		if (monster_level_min == -1) { /* let this function choose, basing on 'level' */
			if (level >= 98) {
				if (table[i].level < 69) continue; /* 64..69 somewhere is a good start for nasty stuff - C. Blue */
			} else {
				if (table[i].level < (level * 2) / 3) continue;
			}
		} else if (monster_level_min) { /* picked a value outside of this function already */
			if (table[i].level < monster_level_min) continue;
		}
#endif
#if 1 /* new attempt ^^ with new mon_allowed_chance to fix above problem */
		if (monster_level_min == -1) { /* let this function choose, basing on 'level' */
			if (level >= 98) {
				if (table[i].level < 69) continue; /* 64..69 somewhere is a good start for nasty stuff - C. Blue */
			} else {
				if (table[i].level < (level * 2) / 3) continue;
			}
		} else if (monster_level_min) { /* picked a value outside of this function already */
			if (table[i].level < monster_level_min) continue;
		}

		hack_prob = mon_allowed_chance(r_ptr);
		if (FALSE) /* neutralize the following mon_allowed check! */
#endif

		if(!mon_allowed(r_ptr)) continue;

		/* Some dungeon types restrict the possible monsters */
//		if(!summon_hack && !restrict_monster_to_dungeon(r_idx) && dlev) continue;
		if(dun_type && !restrict_monster_to_dungeon(r_idx, dun_type)) continue;


		/* Accept */
		table[i].prob3 = (table[i].prob2 * hack_prob) / 100; /* hack_prob belongs to monster_level_min implementation in this function */

		/* Total */
		total += table[i].prob3;
	}

	/* No legal monsters */
	if (total <= 0) return (0);


	/* Pick a monster */
	value = rand_int(total);

	/* Find the monster */
	for (i = 0; i < alloc_race_size; i++)
	{
		/* Found the entry */
		if (value < table[i].prob3) break;

		/* Decrement */
		value = value - table[i].prob3;
	}


	/* Power boost */
	p = rand_int(100);

	/* Try for a "harder" monster once (50%) or twice (10%) */
	if (p < 60)
	{
		/* Save old */
		j = i;

		/* Pick a monster */
		value = rand_int(total);

		/* Find the monster */
		for (i = 0; i < alloc_race_size; i++)
		{
			/* Found the entry */
			if (value < table[i].prob3) break;

			/* Decrement */
			value = value - table[i].prob3;
		}

		/* Keep the "best" one */
		if (abs(table[i].level) < abs(table[j].level)) i = j;
	}

	/* Try for a "harder" monster twice (10%) */
	if (p < 10)
	{
		/* Save old */
		j = i;

		/* Pick a monster */
		value = rand_int(total);

		/* Find the monster */
		for (i = 0; i < alloc_race_size; i++)
		{
			/* Found the entry */
			if (value < table[i].prob3) break;

			/* Decrement */
			value = value - table[i].prob3;
		}

		/* Keep the "best" one */
		if (abs(table[i].level) < abs(table[j].level)) i = j;
	}


	/* Result */
	return (table[i].index);
}




static cptr r_name_garbled_get()
{
	int r_idx;

	while (TRUE)
	{
		r_idx = rand_int(MAX_R_IDX);
		if (!r_info[r_idx].name) continue;
        else return (r_name + r_info[r_idx].name);
	}
}


/*
 * Build a string describing a monster in some way.
 *
 * We can correctly describe monsters based on their visibility.
 * We can force all monsters to be treated as visible or invisible.
 * We can build nominatives, objectives, possessives, or reflexives.
 * We can selectively pronominalize hidden, visible, or all monsters.
 * We can use definite or indefinite descriptions for hidden monsters.
 * We can use definite or indefinite descriptions for visible monsters.
 *
 * Pronominalization involves the gender whenever possible and allowed,
 * so that by cleverly requesting pronominalization / visibility, you
 * can get messages like "You hit someone.  She screams in agony!".
 *
 * Reflexives are acquired by requesting Objective plus Possessive.
 *
 * If no m_ptr arg is given (?), the monster is assumed to be hidden,
 * unless the "Assume Visible" mode is requested.
 *
 * If no r_ptr arg is given, it is extracted from m_ptr and r_info
 * If neither m_ptr nor r_ptr is given, the monster is assumed to
 * be neuter, singular, and hidden (unless "Assume Visible" is set),
 * in which case you may be in trouble... :-)
 *
 * I am assuming that no monster name is more than 70 characters long,
 * so that "char desc[80];" is sufficiently large for any result.
 *
 * Mode Flags:
 *   0x01 --> Objective (or Reflexive)
 *   0x02 --> Possessive (or Reflexive)
 *   0x04 --> Use indefinites for hidden monsters ("something")
 *   0x08 --> Use indefinites for visible monsters ("a kobold")
 *   0x10 --> Pronominalize hidden monsters
 *   0x20 --> Pronominalize visible monsters
 *   0x40 --> Assume the monster is hidden
 *   0x80 --> Assume the monster is visible
 *
 * Useful Modes:
 *   0x00 --> Full nominative name ("the kobold") or "it"
 *   0x04 --> Full nominative name ("the kobold") or "something"
 *   0x80 --> Genocide resistance name ("the kobold")
 *   0x88 --> Killing name ("a kobold")
 *   0x22 --> Possessive, genderized if visable ("his") or "its"
 *   0x23 --> Reflexive, genderized if visable ("himself") or "itself"
 */
/*
 * ToME-NET Extra Flags:
 * 0x0100 --> Ban 'Garbled' name (for death-record)
 */
/* FIXME: 'The The Borshin' when hallucinating */
void monster_desc(int Ind, char *desc, int m_idx, int mode)
{
	player_type *p_ptr;

	cptr            res;

        monster_type    *m_ptr = &m_list[m_idx];
        monster_race    *r_ptr = race_inf(m_ptr);

	cptr            name = r_name_get(m_ptr);

	bool            seen, pron;

	/* Check for bad player number */
	if (Ind > 0)
	{
		p_ptr = Players[Ind];
	
		/* Can we "see" it (exists + forced, or visible + not unforced) */
		seen = (m_ptr && ((mode & 0x80) || (!(mode & 0x40) && p_ptr->mon_vis[m_idx])));

		if (!(mode & 0x0100) && p_ptr->image)
			name = r_name_garbled_get();
	}
	else
	{
		seen = (m_ptr && ((mode & 0x80) || (!(mode & 0x40))));
	}

	/* Sexed Pronouns (seen and allowed, or unseen and allowed) */
	pron = (m_ptr && ((seen && (mode & 0x20)) || (!seen && (mode & 0x10))));


	/* First, try using pronouns, or describing hidden monsters */
	if (!seen || pron)
	{
		/* an encoding of the monster "sex" */
		int kind = 0x00;

		/* Extract the gender (if applicable) */
		if (r_ptr->flags1 & RF1_FEMALE) kind = 0x20;
		else if (r_ptr->flags1 & RF1_MALE) kind = 0x10;

		/* Ignore the gender (if desired) */
		if (!m_ptr || !pron) kind = 0x00;


		/* Assume simple result */
		res = "it";

		/* Brute force: split on the possibilities */
		switch (kind + (mode & 0x07))
		{
			/* Neuter, or unknown */
			case 0x00: res = "it"; break;
			case 0x01: res = "it"; break;
			case 0x02: res = "its"; break;
			case 0x03: res = "itself"; break;
			case 0x04: res = "something"; break;
			case 0x05: res = "something"; break;
			case 0x06: res = "something's"; break;
			case 0x07: res = "itself"; break;

			/* Male (assume human if vague) */
			case 0x10: res = "he"; break;
			case 0x11: res = "him"; break;
			case 0x12: res = "his"; break;
			case 0x13: res = "himself"; break;
			case 0x14: res = "someone"; break;
			case 0x15: res = "someone"; break;
			case 0x16: res = "someone's"; break;
			case 0x17: res = "himself"; break;

			/* Female (assume human if vague) */
			case 0x20: res = "she"; break;
			case 0x21: res = "her"; break;
			case 0x22: res = "her"; break;
			case 0x23: res = "herself"; break;
			case 0x24: res = "someone"; break;
			case 0x25: res = "someone"; break;
			case 0x26: res = "someone's"; break;
			case 0x27: res = "herself"; break;
		}

		/* Copy the result */
		(void)strcpy(desc, res);
	}


	/* Handle visible monsters, "reflexive" request */
	else if ((mode & 0x02) && (mode & 0x01))
	{
		/* The monster is visible, so use its gender */
		if (r_ptr->flags1 & RF1_FEMALE) strcpy(desc, "herself");
		else if (r_ptr->flags1 & RF1_MALE) strcpy(desc, "himself");
		else strcpy(desc, "itself");
	}


	/* Handle all other visible monster requests */
	else
	{
		/* It could be a Unique */
		if (r_ptr->flags1 & RF1_UNIQUE)
		{
			/* Start with the name (thus nominative and objective) */
			(void)strcpy(desc, name);
		}

		/* It could be an indefinite monster */
		else if (mode & 0x08)
		{
			/* XXX Check plurality for "some" */

			/* Indefinite monsters need an indefinite article */
			(void)strcpy(desc, is_a_vowel(name[0]) ? "an " : "a ");
			(void)strcat(desc, name);
		}

		/* It could be a normal, definite, monster */
		else
		{
			/* Definite monsters need a definite article */
			(void)strcpy(desc, "the ");
			(void)strcat(desc, name);
		}

		/* Handle the Possessive as a special afterthought */
		if (mode & 0x02)
		{
			/* XXX Check for trailing "s" */

			/* Simply append "apostrophe" and "s" */
			(void)strcat(desc, "'s");
		}
	}
}

void monster_race_desc(int Ind, char *desc, int r_idx, int mode)
{
	player_type *p_ptr;

	cptr            res;

        monster_race    *r_ptr = &r_info[r_idx];

	cptr            name = r_name + r_ptr->name;

	bool            seen = FALSE, pron = FALSE;

	/* Check for bad player number */
	if (Ind > 0)
	{
		p_ptr = Players[Ind];
	
		if (!(mode & 0x0100) && p_ptr->image)
			name = r_name_garbled_get();
			seen = TRUE;
	}
	else
	{
		seen = ((mode & 0x80) || (!(mode & 0x40)));
	}

	/* Sexed Pronouns (seen and allowed, or unseen and allowed) */
	pron = ((seen && (mode & 0x20)) || (!seen && (mode & 0x10)));


	/* First, try using pronouns, or describing hidden monsters */
	if (!seen || pron)
	{
		/* an encoding of the monster "sex" */
		int kind = 0x00;

		/* Extract the gender (if applicable) */
		if (r_ptr->flags1 & RF1_FEMALE) kind = 0x20;
		else if (r_ptr->flags1 & RF1_MALE) kind = 0x10;

		/* Ignore the gender (if desired) */
		if (!pron) kind = 0x00;


		/* Assume simple result */
		res = "it";

		/* Brute force: split on the possibilities */
		switch (kind + (mode & 0x07))
		{
			/* Neuter, or unknown */
			case 0x00: res = "it"; break;
			case 0x01: res = "it"; break;
			case 0x02: res = "its"; break;
			case 0x03: res = "itself"; break;
			case 0x04: res = "something"; break;
			case 0x05: res = "something"; break;
			case 0x06: res = "something's"; break;
			case 0x07: res = "itself"; break;

			/* Male (assume human if vague) */
			case 0x10: res = "he"; break;
			case 0x11: res = "him"; break;
			case 0x12: res = "his"; break;
			case 0x13: res = "himself"; break;
			case 0x14: res = "someone"; break;
			case 0x15: res = "someone"; break;
			case 0x16: res = "someone's"; break;
			case 0x17: res = "himself"; break;

			/* Female (assume human if vague) */
			case 0x20: res = "she"; break;
			case 0x21: res = "her"; break;
			case 0x22: res = "her"; break;
			case 0x23: res = "herself"; break;
			case 0x24: res = "someone"; break;
			case 0x25: res = "someone"; break;
			case 0x26: res = "someone's"; break;
			case 0x27: res = "herself"; break;
		}

		/* Copy the result */
		(void)strcpy(desc, res);
	}


	/* Handle visible monsters, "reflexive" request */
	else if ((mode & 0x02) && (mode & 0x01))
	{
		/* The monster is visible, so use its gender */
		if (r_ptr->flags1 & RF1_FEMALE) strcpy(desc, "herself");
		else if (r_ptr->flags1 & RF1_MALE) strcpy(desc, "himself");
		else strcpy(desc, "itself");
	}


	/* Handle all other visible monster requests */
	else
	{
		/* It could be a Unique */
		if (r_ptr->flags1 & RF1_UNIQUE)
		{
			/* Start with the name (thus nominative and objective) */
			(void)strcpy(desc, name);
		}

		/* It could be an indefinite monster */
		else if (mode & 0x08)
		{
			/* XXX Check plurality for "some" */

			/* Indefinite monsters need an indefinite article */
			(void)strcpy(desc, is_a_vowel(name[0]) ? "an " : "a ");
			(void)strcat(desc, name);
		}

		/* It could be a normal, definite, monster */
		else
		{
			/* Definite monsters need a definite article */
			(void)strcpy(desc, "the ");
			(void)strcat(desc, name);
		}

		/* Handle the Possessive as a special afterthought */
		if (mode & 0x02)
		{
			/* XXX Check for trailing "s" */

			/* Simply append "apostrophe" and "s" */
			(void)strcat(desc, "'s");
		}
	}
}

/* Similar to monster_desc, but it handles the players instead. - Jir - */
void player_desc(int Ind, char *desc, int Ind2, int mode)
{
	player_type *p_ptr, *q_ptr = Players[Ind2];

	cptr            res;

	cptr            name = q_ptr->name;

	bool            seen, pron;

	/* Check for bad player number */
	if (Ind > 0)
	{
		p_ptr = Players[Ind];
	
		/* Can we "see" it (exists + forced, or visible + not unforced) */
		seen = (q_ptr && ((mode & 0x80) || (!(mode & 0x40) && p_ptr->play_vis[Ind2])));

		if (!(mode & 0x0100) && p_ptr->image)
			name = r_name_garbled_get();
	}
	else
	{
		seen = (q_ptr && ((mode & 0x80) || (!(mode & 0x40))));
	}

	/* Sexed Pronouns (seen and allowed, or unseen and allowed) */
	pron = (q_ptr && ((seen && (mode & 0x20)) || (!seen && (mode & 0x10))));


	/* First, try using pronouns, or describing hidden monsters */
	if (!seen || pron)
	{
		/* an encoding of the monster "sex" */
		int kind = 0x00;

		/* Extract the gender (if applicable) */
		if (q_ptr->male) kind = 0x10;
		else kind = 0x20;

		/* Ignore the gender (if desired) */
		if (!q_ptr || !pron) kind = 0x00;


		/* Assume simple result */
		res = "it";

		/* Brute force: split on the possibilities */
		switch (kind + (mode & 0x07))
		{
			/* Neuter, or unknown */
			case 0x00: res = "it"; break;
			case 0x01: res = "it"; break;
			case 0x02: res = "its"; break;
			case 0x03: res = "itself"; break;
			case 0x04: res = "something"; break;
			case 0x05: res = "something"; break;
			case 0x06: res = "something's"; break;
			case 0x07: res = "itself"; break;

			/* Male (assume human if vague) */
			case 0x10: res = "he"; break;
			case 0x11: res = "him"; break;
			case 0x12: res = "his"; break;
			case 0x13: res = "himself"; break;
			case 0x14: res = "someone"; break;
			case 0x15: res = "someone"; break;
			case 0x16: res = "someone's"; break;
			case 0x17: res = "himself"; break;

			/* Female (assume human if vague) */
			case 0x20: res = "she"; break;
			case 0x21: res = "her"; break;
			case 0x22: res = "her"; break;
			case 0x23: res = "herself"; break;
			case 0x24: res = "someone"; break;
			case 0x25: res = "someone"; break;
			case 0x26: res = "someone's"; break;
			case 0x27: res = "herself"; break;
		}

		/* Copy the result */
		(void)strcpy(desc, res);
	}


	/* Handle visible monsters, "reflexive" request */
	else if ((mode & 0x02) && (mode & 0x01))
	{
		/* The monster is visible, so use its gender */
		if (!q_ptr->male) strcpy(desc, "herself");
		else strcpy(desc, "himself");
//		else strcpy(desc, "itself");
	}


	/* Handle all other visible monster requests */
	else
	{
		/* Start with the name (thus nominative and objective) */
		(void)strcpy(desc, name);

		/* Handle the Possessive as a special afterthought */
		if (mode & 0x02)
		{
			/* XXX Check for trailing "s" */

			/* Simply append "apostrophe" and "s" */
			(void)strcat(desc, "'s");
		}
	}
}



/*
 * Learn about a monster (by "probing" it)
 */
void lore_do_probe(int m_idx)
{
	monster_type *m_ptr = &m_list[m_idx];

        monster_race *r_ptr = race_inf(m_ptr);

	/* Hack -- Memorize some flags */
	r_ptr->r_flags1 = r_ptr->flags1;
	r_ptr->r_flags2 = r_ptr->flags2;
	r_ptr->r_flags3 = r_ptr->flags3;

	/* Window stuff */
	/*p_ptr->window |= (PW_MONSTER);*/
}


/*
 * Take note that the given monster just dropped some treasure
 *
 * Note that learning the "GOOD"/"GREAT" flags gives information
 * about the treasure (even when the monster is killed for the first
 * time, such as uniques, and the treasure has not been examined yet).
 *
 * This "indirect" method is used to prevent the player from learning
 * exactly how much treasure a monster can drop from observing only
 * a single example of a drop.  This method actually observes how much
 * gold and items are dropped, and remembers that information to be
 * described later by the monster recall code.
 */
void lore_treasure(int m_idx, int num_item, int num_gold)
{
	monster_type *m_ptr = &m_list[m_idx];

        monster_race *r_ptr = race_inf(m_ptr);

	/* Note the number of things dropped */
	if (num_item > r_ptr->r_drop_item) r_ptr->r_drop_item = num_item;
	if (num_gold > r_ptr->r_drop_gold) r_ptr->r_drop_gold = num_gold;

	/* Hack -- memorize the good/great flags */
	if (r_ptr->flags1 & RF1_DROP_GOOD) r_ptr->r_flags1 |= RF1_DROP_GOOD;
	if (r_ptr->flags1 & RF1_DROP_GREAT) r_ptr->r_flags1 |= RF1_DROP_GREAT;
}

/*
 * Here comes insanity from PernAngband.. hehehe.. - Jir -
 */
//void sanity_blast(int Ind, monster_type * m_ptr, bool necro)
static void sanity_blast(int Ind, int m_idx, bool necro)
{
	player_type *p_ptr = Players[Ind];
	monster_type    *m_ptr = &m_list[m_idx];
	bool happened = FALSE;
	int power = 100;

	/* Don't blast the master */
	if (p_ptr->admin_dm) return;

	if (!necro)
	{
		char            m_name[80];
		monster_race    *r_ptr;

		if (m_ptr != NULL) r_ptr = race_inf(m_ptr);
		else return;

		power = (m_ptr->level)+10;

		monster_desc(Ind, m_name, m_idx, 0);

		if (!(r_ptr->flags1 & RF1_UNIQUE))
		{
			if (r_ptr->flags1 & RF1_FRIENDS)
				power /= 2;
		}
		else power *= 2;

		/* No effect yet, just loaded... */
		//		if (!hack_mind) return;

		//		if (!(m_ptr->ml))
		if (!p_ptr->mon_vis[m_idx])
			return; /* Cannot see it for some reason */

		if (!(r_ptr->flags2 & RF2_ELDRITCH_HORROR))
			return; /* oops */



		/* Pet eldritch horrors are safe most of the time */
		//                if ((is_friend(m_ptr) > 0) && (randint(8)!=1)) return;


		if (randint(power)<p_ptr->skill_sav)
		{
			return; /* Save, no adverse effects */
		}


		if (p_ptr->image)
		{
			/* Something silly happens... */
			msg_format(Ind, "You behold the %s visage of %s!",
					funny_desc[(randint(MAX_FUNNY))-1], m_name);
			if (randint(3)==1)
			{
				msg_print(Ind, funny_comments[randint(MAX_COMMENT)-1]);
				p_ptr->image = (p_ptr->image + randint(m_ptr->level));
			}
			return; /* Never mind; we can't see it clearly enough */
		}

		/* Something frightening happens... */
		msg_format(Ind, "You behold the %s visage of %s!",
				horror_desc[(randint(MAX_HORROR))-1], m_name);

		r_ptr->r_flags2 |= RF2_ELDRITCH_HORROR;


		/* Undead characters are 50% likely to be unaffected */
#if 0
		if ((PRACE_FLAG(PR1_UNDEAD))||(p_ptr->mimic_form == MIMIC_VAMPIRE))
		{
			if (randint(100) < (25 + (p_ptr->lev))) return;
		}
#endif	// 0
	}
	else
	{
		msg_print(Ind, "Your sanity is shaken by reading the Necronomicon!");
	}

	if (randint(power)<p_ptr->skill_sav) /* Mind blast */
	{
		if (!p_ptr->resist_conf)
		{
			(void)set_confused(Ind, p_ptr->confused + rand_int(4) + 4);
		}
		if ((!p_ptr->resist_chaos) && (randint(3)==1))
		{
			(void) set_image(Ind, p_ptr->image + rand_int(250) + 150);
		}
		return;
	}

	if (randint(power)<p_ptr->skill_sav) /* Lose int & wis */
	{
		do_dec_stat (Ind, A_INT, STAT_DEC_NORMAL);
		do_dec_stat (Ind, A_WIS, STAT_DEC_NORMAL);
		return;
	}


	if (randint(power)<p_ptr->skill_sav) /* Brain smash */
	{
		if (!p_ptr->resist_conf)
		{
			(void)set_confused(Ind, p_ptr->confused + rand_int(4) + 4);
		}
		if (!p_ptr->free_act)
		{
			(void)set_paralyzed(Ind, p_ptr->paralyzed + rand_int(4) + 4);
		}
		while (rand_int(100) > p_ptr->skill_sav)
			(void)do_dec_stat(Ind, A_INT, STAT_DEC_NORMAL);	
		while (rand_int(100) > p_ptr->skill_sav)
			(void)do_dec_stat(Ind, A_WIS, STAT_DEC_NORMAL);
		if (!p_ptr->resist_chaos)
		{
			(void) set_image(Ind, p_ptr->image + rand_int(250) + 150);
		}
		return;
	}

	if (randint(power)<p_ptr->skill_sav) /* Permanent lose int & wis */
	{
		if (dec_stat(Ind, A_INT, 10, TRUE)) happened = TRUE;
		if (dec_stat(Ind, A_WIS, 10, TRUE)) happened = TRUE;
		if (happened)
			msg_print(Ind, "You feel much less sane than before.");
		return;
	}


	if (randint(power)<p_ptr->skill_sav) /* Amnesia */
	{

		if (lose_all_info(Ind))
			msg_print(Ind, "You forget everything in your utmost terror!");
		return;
	}

	/* Else gain permanent insanity */
#if 0
	if ((p_ptr->muta3 & MUT3_MORONIC) && (p_ptr->muta2 & MUT2_BERS_RAGE) &&
			((p_ptr->muta2 & MUT2_COWARDICE) || (p_ptr->resist_fear)) &&
			((p_ptr->muta2 & MUT2_HALLU) || (p_ptr->resist_chaos)))
	{
		/* The poor bastard already has all possible insanities! */
		return;
	}

	while (!happened)
	{
		switch(randint(4))
		{
			case 1:
				if (!(p_ptr->muta3 & MUT3_MORONIC))
				{
					msg_print("You turn into an utter moron!");
					if (p_ptr->muta3 & MUT3_HYPER_INT)
					{
						msg_print("Your brain is no longer a living computer.");
						p_ptr->muta3 &= ~(MUT3_HYPER_INT);
					}
					p_ptr->muta3 |= MUT3_MORONIC;
					happened = TRUE;
				}
				break;
			case 2:
				if (!(p_ptr->muta2 & MUT2_COWARDICE) && !(p_ptr->resist_fear))
				{
					msg_print("You become paranoid!");

					/* Duh, the following should never happen, but anyway... */
					if (p_ptr->muta3 & MUT3_FEARLESS)
					{
						msg_print("You are no longer fearless.");
						p_ptr->muta3 &= ~(MUT3_FEARLESS);
					}

					p_ptr->muta2 |= MUT2_COWARDICE;
					happened = TRUE;
				}
				break;
			case 3:
				if (!(p_ptr->muta2 & MUT2_HALLU) && !(p_ptr->resist_chaos))
				{
					msg_print("You are afflicted by a hallucinatory insanity!");
					p_ptr->muta2 |= MUT2_HALLU;
					happened = TRUE;
				}
				break;
			default:
				if (!(p_ptr->muta2 & MUT2_BERS_RAGE))
				{
					msg_print("You become subject to fits of berserk rage!");
					p_ptr->muta2 |= MUT2_BERS_RAGE;
					happened = TRUE;
				}
				break;
		}
	}
#endif	// 0

	p_ptr->update |= PU_BONUS;
	handle_stuff(Ind);
}



/*
 * This function updates the monster record of the given monster
 *
 * This involves extracting the distance to the player, checking
 * for visibility (natural, infravision, see-invis, telepathy),
 * updating the monster visibility flag, redrawing or erasing the
 * monster when the visibility changes, and taking note of any
 * "visual" features of the monster (cold-blooded, invisible, etc).
 *
 * The only monster fields that are changed here are "cdis" (the
 * distance from the player), "los" (clearly visible to player),
 * and "ml" (visible to the player in any way).
 *
 * There are a few cases where the calling routine knows that the
 * distance from the player to the monster has not changed, and so
 * we have a special parameter to request distance computation.
 * This lets many calls to this function run very quickly.
 *
 * Note that every time a monster moves, we must call this function
 * for that monster, and update distance.  Note that every time the
 * player moves, we must call this function for every monster, and
 * update distance.  Note that every time the player "state" changes
 * in certain ways (including "blindness", "infravision", "telepathy",
 * and "see invisible"), we must call this function for every monster.
 *
 * The routines that actually move the monsters call this routine
 * directly, and the ones that move the player, or notice changes
 * in the player state, call "update_monsters()".
 *
 * Routines that change the "illumination" of grids must also call
 * this function, since the "visibility" of some monsters may be
 * based on the illumination of their grid.
 *
 * Note that this function is called once per monster every time the
 * player moves, so it is important to optimize it for monsters which
 * are far away.  Note the optimization which skips monsters which
 * are far away and were completely invisible last turn.
 *
 * Note the optimized "inline" version of the "distance()" function.
 *
 * Note that only monsters on the current panel can be "visible",
 * and then only if they are (1) in line of sight and illuminated
 * by light or infravision, or (2) nearby and detected by telepathy.
 *
 * The player can choose to be disturbed by several things, including
 * "disturb_move" (monster which is viewable moves in some way), and
 * "disturb_near" (monster which is "easily" viewable moves in some
 * way).  Note that "moves" includes "appears" and "disappears".
 */
void update_mon(int m_idx, bool dist)
{
	monster_type *m_ptr = &m_list[m_idx];

        monster_race *r_ptr = race_inf(m_ptr);

	player_type *p_ptr;

	/* The current monster location */
	int fy = m_ptr->fy;
	int fx = m_ptr->fx;

	struct worldpos *wpos=&m_ptr->wpos;
	cave_type **zcave;

	int Ind = m_ptr->closest_player;

	/* Seen at all */
	bool flag = FALSE;

	/* Seen by vision */
	bool easy = FALSE;

	/* Seen by telepathy */
	bool hard = FALSE;

	/* Various extra flags */
	bool do_empty_mind = FALSE;
	bool do_weird_mind = FALSE;
	bool do_invisible = FALSE;
	bool do_cold_blood = FALSE;

	/* Check for each player */
	for (Ind = 1; Ind < NumPlayers + 1; Ind++)
	{
		p_ptr = Players[Ind];

		/* Reset the flags */
		flag = easy = hard = FALSE;

		/* If he's not playing, skip him */
		if (p_ptr->conn == NOT_CONNECTED)
			continue;

		/* If he's not on this depth, skip him */
		if(!inarea(wpos, &p_ptr->wpos))
		{
			p_ptr->mon_vis[m_idx] = FALSE;
			p_ptr->mon_los[m_idx] = FALSE;
			continue;
		}

		/* If our wilderness level has been deallocated, stop here...
		 * we are "detatched" monsters.
		 */

		if(!(zcave=getcave(wpos))) continue;

		/* Calculate distance */
		if (dist)
		{
			int d, dy, dx;

			/* Distance components */
			dy = (p_ptr->py > fy) ? (p_ptr->py - fy) : (fy - p_ptr->py);
			dx = (p_ptr->px > fx) ? (p_ptr->px - fx) : (fx - p_ptr->px);

			/* Approximate distance */
			d = (dy > dx) ? (dy + (dx>>1)) : (dx + (dy>>1));

			/* Save the distance (in a byte) */
			m_ptr->cdis = (d < 255) ? d : 255;
		}


		/* Process "distant" monsters */
#if 0
		if (m_ptr->cdis > MAX_SIGHT)
		{
			/* Ignore unseen monsters */
			if (!p_ptr->mon_vis[m_idx]) return;
		}
#endif

		/* Process "nearby" monsters on the current "panel" */
		if (panel_contains(fy, fx))
		{
			cave_type *c_ptr = &zcave[fy][fx];
			byte *w_ptr = &p_ptr->cave_flag[fy][fx];

			/* Normal line of sight, and player is not blind */
			if ((*w_ptr & CAVE_VIEW) && (!p_ptr->blind))
			{
				/* The monster is carrying/emitting light? */
				if ((r_ptr->flags9 & RF9_HAS_LITE) &&
						!(m_ptr->csleep) &&
						!(r_ptr->flags2 & RF2_INVISIBLE)) easy = flag = TRUE;

				/* Use "infravision" */
				if (m_ptr->cdis <= (byte)(p_ptr->see_infra))
				{
					/* Infravision only works on "warm" creatures */
					/* Below, we will need to know that infravision failed */
					if (r_ptr->flags2 & RF2_COLD_BLOOD) do_cold_blood = TRUE;

					/* Infravision works */
					if (!do_cold_blood) easy = flag = TRUE;
				}

				/* Use "illumination" */
				if ((c_ptr->info & CAVE_LITE) || (c_ptr->info & CAVE_GLOW))
				{
					/* Take note of invisibility */
					if (r_ptr->flags2 & RF2_INVISIBLE) do_invisible = TRUE;

					/* Visible, or detectable, monsters get seen */
					if (!do_invisible || p_ptr->see_inv) easy = flag = TRUE;
				}
			}

			/* Telepathy can see all "nearby" monsters with "minds" */
			if (p_ptr->telepathy || (p_ptr->prace == RACE_DRIDER))
			{
				bool see = FALSE, drsee=FALSE;

				/* Different ESP */
				if (p_ptr->prace==RACE_DRIDER) drsee = TRUE;
				if ((p_ptr->telepathy & ESP_ORC) && (r_ptr->flags3 & RF3_ORC)) see = TRUE;
				if ((p_ptr->telepathy & ESP_SPIDER) && (r_ptr->flags7 & RF7_SPIDER)) see = TRUE;
				if ((p_ptr->telepathy & ESP_TROLL) && (r_ptr->flags3 & RF3_TROLL)) see = TRUE;
				if ((p_ptr->telepathy & ESP_DRAGON) && (r_ptr->flags3 & RF3_DRAGON)) see = TRUE;
				if ((p_ptr->telepathy & ESP_DRAGON) && (r_ptr->flags3 & RF3_DRAGONRIDER)) see = TRUE;
				if ((p_ptr->telepathy & ESP_GIANT) && (r_ptr->flags3 & RF3_GIANT)) see = TRUE;
				if ((p_ptr->telepathy & ESP_DEMON) && (r_ptr->flags3 & RF3_DEMON)) see = TRUE;
				if ((p_ptr->telepathy & ESP_UNDEAD) && (r_ptr->flags3 & RF3_UNDEAD)) see = TRUE;
				if ((p_ptr->telepathy & ESP_EVIL) && (r_ptr->flags3 & RF3_EVIL)) see = TRUE;
				if ((p_ptr->telepathy & ESP_ANIMAL) && (r_ptr->flags3 & RF3_ANIMAL)) see = TRUE;
				if ((p_ptr->telepathy & ESP_DRAGONRIDER) && (r_ptr->flags3 & RF3_DRAGONRIDER)) see = TRUE;
				if ((p_ptr->telepathy & ESP_GOOD) && (r_ptr->flags3 & RF3_GOOD)) see = TRUE;
				if ((p_ptr->telepathy & ESP_NONLIVING) && (r_ptr->flags3 & RF3_NONLIVING)) see = TRUE;
				if ((p_ptr->telepathy & ESP_UNIQUE) && ((r_ptr->flags1 & RF1_UNIQUE) || (r_ptr->flags3 & RF3_UNIQUE_4))) see = TRUE;
				if (p_ptr->telepathy & ESP_ALL) see = TRUE;

//				if (p_ptr->mode == MODE_NORMAL) see = TRUE;
				if (see && (p_ptr->mode & MODE_HARD) && (m_ptr->cdis > MAX_SIGHT)) see = FALSE;
//				if (see && !p_ptr->telepathy && (p_ptr->prace == RACE_DRIDER) && (m_ptr->cdis > (p_ptr->lev / 2))) see = FALSE;
				if (drsee && !see){
//					if(p_ptr->lev>=6 && m_ptr->cdis<=(5+p_ptr->lev/2)) see=TRUE;
					/* They receive 'fly' instead */
					if(p_ptr->lev>=6 && m_ptr->cdis<=(5+p_ptr->lev/3)) see=TRUE;
				}

				if (see)
				{
					/* Empty mind, no telepathy */
					if (r_ptr->flags2 & RF2_EMPTY_MIND)
					{
						do_empty_mind = TRUE;
					}
	
					/* Weird mind, occasional telepathy */
					else if (r_ptr->flags2 & RF2_WEIRD_MIND)
					{
						do_weird_mind = TRUE;
						if (rand_int(100) < 10) hard = flag = TRUE;
					}

					/* Normal mind, allow telepathy */
					else
					{
						hard = flag = TRUE;
					}

					/* Apply telepathy */
					if (hard)
					{
						/* Hack -- Memorize mental flags */
						if (r_ptr->flags2 & RF2_SMART) r_ptr->r_flags2 |= RF2_SMART;
						if (r_ptr->flags2 & RF2_STUPID) r_ptr->r_flags2 |= RF2_STUPID;
					}
				}
			}

			/* Hack -- Wizards have "perfect telepathy" */
			if (p_ptr->admin_dm) flag = TRUE;
			
			/* Arena Monster Challenge event provides wizard-esp too */
			if (ge_special_sector && p_ptr->wpos.wx == WPOS_ARENA_X && p_ptr->wpos.wy == WPOS_ARENA_Y && p_ptr->wpos.wz == WPOS_ARENA_Z)
				flag = TRUE;
		}


		/* The monster is now visible */
		if (flag)
		{
			/* It was previously unseen */
			if (!p_ptr->mon_vis[m_idx])
			{
				/* Mark as visible */
				p_ptr->mon_vis[m_idx] = TRUE;

				/* Draw the monster */
				lite_spot(Ind, fy, fx);

				/* Update health bar as needed */
				if (p_ptr->health_who == m_idx) p_ptr->redraw |= (PR_HEALTH);

				/* Hack -- Count "fresh" sightings */
				if (r_ptr->r_sights < MAX_SHORT) r_ptr->r_sights++;

				/* Disturb on appearance */
				if(!m_list[m_idx].special && r_ptr->d_char != 't' &&
				    /* Not in Bree (for Santa Claus) - C. Blue */
	                    	    (p_ptr->wpos.wx != cfg.town_x || p_ptr->wpos.wy != cfg.town_y || p_ptr->wpos.wz))
					if (p_ptr->disturb_move) disturb(Ind, 1, 0);
			}

			/* Memorize various observable flags */
			if (do_empty_mind) r_ptr->r_flags2 |= RF2_EMPTY_MIND;
			if (do_weird_mind) r_ptr->r_flags2 |= RF2_WEIRD_MIND;
			if (do_cold_blood) r_ptr->r_flags2 |= RF2_COLD_BLOOD;
			if (do_invisible) r_ptr->r_flags2 |= RF2_INVISIBLE;

			/* Efficiency -- Notice multi-hued monsters */
//			if (r_ptr->flags1 & RF1_ATTR_MULTI) scan_monsters = TRUE;
#ifdef RANDUNIS 
			if ((r_ptr->flags1 & RF1_ATTR_MULTI) ||
					(r_ptr->flags1 & RF1_UNIQUE) ||
					(m_ptr->ego ) ) scan_monsters = TRUE;
#else
			if ((r_ptr->flags1 & RF1_ATTR_MULTI) ||
					(r_ptr->flags1 & RF1_UNIQUE) )
				scan_monsters = TRUE;
#endif	// RANDUNIS
		}

		/* The monster is not visible */
		else
		{
			/* It was previously seen */
			if (p_ptr->mon_vis[m_idx])
			{
				/* Mark as not visible */
				p_ptr->mon_vis[m_idx] = FALSE;

				/* Erase the monster */
				lite_spot(Ind, fy, fx);

				/* Update health bar as needed */
				if (p_ptr->health_who == m_idx) p_ptr->redraw |= (PR_HEALTH);

				/* Disturb on disappearance*/
				if(!m_list[m_idx].special && r_ptr->d_char != 't' &&
				    /* Not in Bree (for Santa Claus) - C. Blue */
	                    	    (p_ptr->wpos.wx != cfg.town_x || p_ptr->wpos.wy != cfg.town_y || p_ptr->wpos.wz))
					if (p_ptr->disturb_move) disturb(Ind, 1, 0);
			}
		}


		/* The monster is now easily visible */
		if (easy)
		{
			/* Change */
			if (!p_ptr->mon_los[m_idx])
			{
				/* Mark as easily visible */
				p_ptr->mon_los[m_idx] = TRUE;

				/* Disturb on appearance */
				if(!m_list[m_idx].special && r_ptr->d_char != 't' &&
				    /* Not in Bree (for Santa Claus) - C. Blue */
	                    	    (p_ptr->wpos.wx != cfg.town_x || p_ptr->wpos.wy != cfg.town_y || p_ptr->wpos.wz))
					if (p_ptr->disturb_near) disturb(Ind, 1, 0);

				/* well, is it the right place to be? */
				if (r_ptr->flags2 & RF2_ELDRITCH_HORROR)
				{
					sanity_blast(Ind, m_idx, FALSE);
				}

			}
		}

		/* The monster is not easily visible */
		else
		{
			/* Change */
			if (p_ptr->mon_los[m_idx])
			{
				/* Mark as not easily visible */
				p_ptr->mon_los[m_idx] = FALSE;

				/* Disturb on disappearance */
				if(!m_list[m_idx].special && r_ptr->d_char != 't' &&
				    /* Not in Bree (for Santa Claus) - C. Blue */
	                    	    (p_ptr->wpos.wx != cfg.town_x || p_ptr->wpos.wy != cfg.town_y || p_ptr->wpos.wz))
					if (p_ptr->disturb_near) disturb(Ind, 1, 0);
			}
		}
	}
}




/*
 * This function simply updates all the (non-dead) monsters (see above).
 */
void update_monsters(bool dist)
{
	int          i;

	/* Efficiency -- Clear multihued flag */
	scan_monsters = FALSE;

	/* Update each (live) monster */
	for (i = 1; i < m_max; i++)
	{
		monster_type *m_ptr = &m_list[i];

		/* Skip dead monsters */
		if (!m_ptr->r_idx) continue;

		/* Update the monster */
		update_mon(i, dist);
	}
}


/*
 * This function updates the visiblity flags for everyone who may see
 * this player.
 */
void update_player(int Ind)
{
	player_type *p_ptr, *q_ptr = Players[Ind];

	int i;
	cave_type **zcave;

	/* Current player location */
	int py = q_ptr->py;
	int px = q_ptr->px;

	/* Distance */
	int dis;

	/* Seen at all */
	bool flag = FALSE;

	/* Seen by vision */
	bool easy = FALSE;

	/* Seen by telepathy */
	bool hard = FALSE;

	/* Check for every other player */
	for (i = 1; i <= NumPlayers; i++)
	{
		p_ptr = Players[i];
		zcave=getcave(&p_ptr->wpos);

		/* Skip disconnected players */
		if (p_ptr->conn == NOT_CONNECTED) continue;

		/* Skip players not on this depth */
		if(!inarea(&p_ptr->wpos, &q_ptr->wpos)) continue;

		/* Player can always see himself */
		if (Ind == i) continue;

		/* Reset the flags */
		flag = easy = hard = FALSE;

		/* Compute distance */
		dis = distance(py, px, p_ptr->py, p_ptr->px);

		/* Process players on current panel */
		if (panel_contains(py, px) && zcave)
		{
			cave_type *c_ptr = &zcave[py][px];
			byte *w_ptr = &p_ptr->cave_flag[py][px];

			/* Normal line of sight, and player is not blind */
			
			/* -APD- this line needs to be seperated to support seeing party
			   members who are out of line of sight */
			/* if ((*w_ptr & CAVE_VIEW) && (!p_ptr->blind)) */
			
			if (!p_ptr->blind)                      
			{
			if ((player_in_party(q_ptr->party, i)) && (q_ptr->party)) easy = flag = TRUE;
			
			if (*w_ptr & CAVE_VIEW) {
				/* Check infravision */
				if (dis <= (byte)(p_ptr->see_infra))
				{
					/* Visible */
					easy = flag = TRUE;
				}

				/* Check illumination */
				if ((c_ptr->info & CAVE_LITE) || (c_ptr->info & CAVE_GLOW))
				{
					/* Check for invisibility */
					if (!q_ptr->ghost || p_ptr->see_inv)
					{
						/* Visible */
						easy = flag = TRUE;
					}
				}
				
				} /* end yucky hack */
			}

			/* Telepathy can see all players */
			if (p_ptr->telepathy & ESP_ALL || (p_ptr->prace == RACE_DRIDER))
			{
			  bool see = FALSE;

			  if (!(p_ptr->mode & MODE_HARD)) see = TRUE;
			  if ((p_ptr->mode & MODE_HARD) && (dis < MAX_SIGHT)) see = TRUE;
			  if (!(p_ptr->telepathy&ESP_ALL) && (p_ptr->prace == RACE_DRIDER) && (p_ptr->lev<6 || (dis > (5+p_ptr->lev / 2)))) see = FALSE;

			  if (see)
			    {
				/* Visible */
				hard = flag = TRUE;
			    }
			}
			
			/* Can we see invisible players ? */
			if ((!p_ptr->see_inv ||
			    ((q_ptr->inventory[INVEN_OUTER].k_idx) &&
			    (q_ptr->inventory[INVEN_OUTER].tval == TV_CLOAK) &&
			    (q_ptr->inventory[INVEN_OUTER].sval == SV_SHADOW_CLOAK))) &&
			    q_ptr->invis && !player_in_party(p_ptr->party, Ind))
			{
				if ((q_ptr->lev > p_ptr->lev) || (randint(p_ptr->lev) > (q_ptr->lev / 2)))
					flag = FALSE;
			}
			if (q_ptr->cloaked && !player_in_party(p_ptr->party, Ind)) flag = FALSE;

			/* Dungeon masters can see invisible players */
			if (p_ptr->admin_dm) flag = TRUE;

			/* Arena Monster Challenge event provides wizard-esp too */
			if (ge_special_sector && p_ptr->wpos.wx == WPOS_ARENA_X && p_ptr->wpos.wy == WPOS_ARENA_Y && p_ptr->wpos.wz == WPOS_ARENA_Z)
				flag = TRUE;

			/* PvP-Arena provides wizard-esp too */
			if (p_ptr->wpos.wx == WPOS_PVPARENA_X && p_ptr->wpos.wy == WPOS_PVPARENA_Y && p_ptr->wpos.wz == WPOS_PVPARENA_Z) flag = TRUE;

			/* hack: PvP-mode players can ESP each other */
			if ((p_ptr->mode & MODE_PVP) && (q_ptr->mode & MODE_PVP)) flag = TRUE;

			/* hack -- dungeon masters are invisible */
			if (q_ptr->admin_dm && !player_sees_dm(i)) flag = FALSE;
		}

		/* Player is now visible */
		if (flag)
		{
			/* It was previously unseen */
			if (!p_ptr->play_vis[Ind])
			{
				/* Mark as visible */
				p_ptr->play_vis[Ind] = TRUE;

				/* Draw the player */
				lite_spot(i, py, px);

				/* Disturb on appearance */
				if (p_ptr->disturb_move && check_hostile(i, Ind))
				{
					/* Disturb */
					disturb(i, 1, 0);
				}
			}
		}

		/* The player is not visible */
		else
		{
			/* It was previously seen */
			if (p_ptr->play_vis[Ind])
			{
				/* Mark as not visible */
				p_ptr->play_vis[Ind] = FALSE;

				/* Erase the player */
				lite_spot(i, py, px);

				/* Disturb on disappearance */
				if (p_ptr->disturb_move && check_hostile(i, Ind))
				{
					/* Disturb */
					disturb(i, 1, 0);
				}
			}
		}

		/* The player is now easily visible */
		if (easy)
		{
			/* Change */
			if (!p_ptr->play_los[Ind])
			{
				/* Mark as easily visible */
				p_ptr->play_los[Ind] = TRUE;

				/* Disturb on appearance */
				if (p_ptr->disturb_near && check_hostile(i, Ind))
				{
					/* Disturb */
					disturb(i, 1, 0);
				}
			}
		}

		/* The player is not easily visible */
		else
		{
			/* Change */
			if (p_ptr->play_los[Ind])
			{
				/* Mark as not easily visible */
				p_ptr->play_los[Ind] = FALSE;

				/* Disturb on disappearance */
				if (p_ptr->disturb_near && check_hostile(i, Ind))
				{
					/* Disturb */
					disturb(i, 1, 0);
				}
			}
		}
	}
}

/*
 * This function simply updates all the players (see above).
 */
void update_players(void)
{
	int i;

	/* Update each player */
	for (i = 1; i <= NumPlayers; i++)
	{
		player_type *p_ptr = Players[i];

		/* Skip disconnected players */
		if (p_ptr->conn == NOT_CONNECTED) continue;

		/* Update the player */
		update_player(i);
	}
}


/* Scan all players on the level and see if at least one can find the unique */
static bool allow_unique_level(int r_idx, struct worldpos *wpos)
{
	int i;
	
	for (i = 1; i <= NumPlayers; i++)
	{
		player_type *p_ptr = Players[i];

		/* Is the player on the level and did he killed the unique already ? */
		if (!p_ptr->r_killed[r_idx] && (inarea(wpos, &p_ptr->wpos)))
		{
			/* One is enough */
			return (TRUE);
		}
	}
	
	/* Yeah but we need at least ONE */
	return (FALSE);
}

/*
 * Attempt to place a monster of the given race at the given location.
 *
 * To give the player a sporting chance, any monster that appears in
 * line-of-sight and is extremely dangerous can be marked as
 * "FORCE_SLEEP", which will cause them to be placed with low energy,
 * which often (but not always) lets the player move before they do.
 *
 * This routine refuses to place out-of-depth "FORCE_DEPTH" monsters.
 *
 * XXX XXX XXX Use special "here" and "dead" flags for unique monsters,
 * remove old "cur_num" and "max_num" fields.
 *
 * XXX XXX XXX Actually, do something similar for artifacts, to simplify
 * the "preserve" mode, and to make the "what artifacts" flag more useful.
 */
	/* lots of hard-coded stuff in here -C. Blue */
bool place_monster_one(struct worldpos *wpos, int y, int x, int r_idx, int ego, int randuni, bool slp, int clo, int clone_summoning)
{
	int                     i, Ind, j, m_idx;
	bool			already_on_level = FALSE;

	cave_type               *c_ptr;

	monster_type    *m_ptr;
	monster_race    *r_ptr = &r_info[r_idx];
	player_type	*p_ptr;

	char buf[80];

	cave_type **zcave;
	if(!(zcave=getcave(wpos))) return (FALSE);
	/* Verify location */
	if (!in_bounds(y, x)) return (FALSE);
	/* Require empty space */
	if (!cave_empty_bold(zcave, y, x)) return (FALSE);
	/* Paranoia */
	if (!r_idx) return (FALSE);
	/* Paranoia */
	if (!r_ptr->name) return (FALSE);

if (summon_override_checks != 1) {
	/* require non-protected field. - C. Blue
	    Note that there are two ways (technically) to protect a field:
	    1) use feature 210 for predefined map setups (which is a "protected brown '.' floor tile")
	    2) use CAVE_PROT which can be toggled on runtime as required
	*/
	if (zcave[y][x].info & CAVE_PROT) return (FALSE);
	if (f_info[zcave[y][x].feat].flags1 & FF1_PROTECTED) return (FALSE);
}

/* override all validity checks! (for summoning done by dungeon master) */
if (!summon_override_checks) {

#if 0
	if (!(cave_empty_bold(zcave, y, x) || 
	    (cave_empty_mountain(zcave, y, x) &&
	    ((r_ptr->flags2 && RF2_PASS_WALL) ||
	     (r_ptr->flags8 && RF8_WILD_MOUNTAIN) || 
	     (r_ptr->flags8 && RF8_WILD_VOLCANO))))) return (FALSE);
#endif
	/* Hack -- no creation on glyph of warding */
	if (zcave[y][x].feat == FEAT_GLYPH) return (FALSE);

	if(((!wpos->wz && wild_info[wpos->wy][wpos->wx].radius < 10) || istown(wpos)) && zcave[y][x].info & CAVE_ICKY) return(FALSE);

} /* summon_override_checks == 0 */

#ifdef DEBUG1
if (r_idx == DEBUG1_IDX) s_printf("DEBUG: 1\n");
#endif

/* override all validity checks! (for summoning done by dungeon master) */
if (!summon_override_checks) {

#ifdef RPG_SERVER /* no spawns in Training Tower at all */
	if (wpos->wx == cfg.town_x && wpos->wy == cfg.town_y && wpos->wz > 0) return(FALSE);
#endif

	/* No live spawns allowed in training tower during global event */
	if (ge_special_sector &&
	    wpos->wx == WPOS_ARENA_X && wpos->wy == WPOS_ARENA_Y && 
	    wpos->wz == WPOS_ARENA_Z)
		return(FALSE);

	/* No spawns in 0,0 pvp arena tower */
	if (wpos->wx == WPOS_PVPARENA_X && wpos->wy == WPOS_PVPARENA_Y && wpos->wz == WPOS_PVPARENA_Z) return(FALSE);

	/* No live spawns after initial spawn allowed on NR bottom */
	if (getlevel(wpos) == (166 + 30) && !cave_set_quietly) return(FALSE);

	/* No monster pre-spawns on staircases, to avoid 'pushing off' a player when he goes up/down. */
	if (cave_set_quietly && (
	    zcave[y][x].feat == FEAT_WAY_LESS ||
	    zcave[y][x].feat == FEAT_WAY_MORE ||
	    zcave[y][x].feat == FEAT_LESS ||
	    zcave[y][x].feat == FEAT_MORE)) return(FALSE);

	/* Special hack - bottom of NR is empty except for Zu-Aon */
	if (getlevel(wpos) == (166 + 30)) r_idx = 1097;

	/* Another special hack - No monster spawn in Valinor, except for.. */
	if ((getlevel(wpos) == 200) &&
	    (r_idx != 1100 ) && (r_idx != 1098)) /* Brightlance, Orome */
		return(FALSE);

	/* Wight-King of the Barrow-downs might not occur anywhere else */
	if ((r_idx == 971) && ((wpos->wx != cfg.town_x) || (wpos->wy != cfg.town_y))) return (FALSE);
	/* Hellraiser and Nether Realm minions may only occur in the Nether Realm  */
	/* Hellraiser may not occur right on the 1st floor of the Nether Realm */
	if ((r_idx == 1067) && (getlevel(wpos) < (166 + 1))) return (FALSE);
	/* Dor may not occur on 'easier' (lol) NR levels */
	if ((r_idx == 1085) && (getlevel(wpos) < (166 + 9))) return (FALSE);
	/* Couple of Nether Realm-only monsters hardcoded here */
	if (((r_idx == 1068) || (r_idx == 1080) || (r_idx == 1083) || (r_idx == 1084)) &&
	    (getlevel(wpos) < 166)) return (FALSE);
	/* Zu-Aon guards the bottom of the Nether Realm now */
	if ((r_idx == 1097) && (getlevel(wpos) != (166 + 30))) return (FALSE);
	/* On Nether Realm bottom no Nether Guards but only Zu-Aon may spawn */
	if ((r_idx == 1068) && (getlevel(wpos) == (166 + 30))) r_idx = 1097;

	/* Update r_ptr due to possible r_idx changes */
	r_ptr = &r_info[r_idx];

	/* Nether Guard isn't a unique but there's only 1 guard per level,
	   If Zu-Aon appears, the Nether Guard disappears instead */
	if (r_idx == 1068) {
#if DEBUG_LEVEL > 2
		s_printf("Checking for old Nether Guards\n");
#endif
		for (i = m_top - 1; i >= 0; i--)
		{
	        	m_idx = m_fast[i];
			m_ptr = &m_list[m_idx];
			if (!m_ptr->r_idx) {
				m_fast[i] = m_fast[--m_top];
				continue;
			}
			/* Old Nether Guard on this level are deleted */
			if ((m_ptr->r_idx == 1068) && inarea(wpos, &m_ptr->wpos)) return(FALSE);
		}
	}

	/* Morgoth may not spawn 'live' if the players on his level aren't prepared correctly */
	/* Morgoth may not spawn 'live' at all (!) if MORGOTH_NO_TELE_VAULTS is defined!
	   (works in conjunction with cave_gen in generate.c) */
	if (r_idx == 862) {
#ifdef MORGOTH_NO_LIVE_SPAWN
		/* is Morgoth not generated within a dungeon level's
		   initialization (cave_gen in generate.c) ? */
		if (!cave_set_quietly) {
			/* No, it's a live spawn! (!cave_set_quietly) */
 #if DEBUG_LEVEL > 2
		        s_printf("Morgoth live spawn prevented (MORGOTH_NO_TELE_VAULTS)\n");
 #endif
			/* Prevent that. */
			return (FALSE);
		} else {
#endif
		for (i = 1; i <= NumPlayers; i++)
		{
			p_ptr = Players[i];
			if (is_admin(p_ptr)) continue;
			if (inarea(&p_ptr->wpos, wpos) &&
			    (p_ptr->total_winner || (p_ptr->r_killed[860] == 0)))
			{
			        /* log */
 #if DEBUG_LEVEL > 2
				if (cave_set_quietly) {
					if (p_ptr->total_winner) {
			    		        s_printf("Morgoth generation prevented due to winner %s\n", p_ptr->name);
					} else {
					        s_printf("Morgoth generation prevented due to Sauron-misser %s\n", p_ptr->name);
					}
				} else {
					if (p_ptr->total_winner) {
			    		        s_printf("Morgoth live spawn prevented due to winner %s\n", p_ptr->name);
					} else {
					        s_printf("Morgoth live spawn prevented due to Sauron-misser %s\n", p_ptr->name);
					}
				}
 #endif
				return (FALSE);
			}
		}
#ifdef MORGOTH_NO_LIVE_SPAWN
		}
#endif
	}

	/* Morgoth shouldn't spawn on NO_DESTROY levels,
	   since his earthquakes are an important weapon! */
#if 0 /* His quakes now override LF1_NO_DESTROY ! */
	if ((r_idx == 862) && (getfloor(wpos)->flags1 & LF1_NO_DESTROY)) {
#if DEBUG_LEVEL > 2
		s_printf("Morgoth spawn prevented on NO_DESTROY level\n");
#endif
		return (FALSE);
	}
#endif

#ifdef DEBUG1
if (r_idx == DEBUG1_IDX) s_printf("DEBUG1: 2\n");
#endif

	/* Hack -- "unique" monsters may not appear on the world surface */
	if ((r_ptr->flags1 & RF1_UNIQUE) && (wpos->wz == 0)) return(FALSE);

/* BEGIN of ugly hack */
	/* Check if the monster is already on the level -
	   I put this in after someone told me about 3 Glaurungs on the same
	   level, while 1 player on the depth had him killed, the other didn't.
	   Then a Glaurung summoned 2 more of himself.. */

        for(i=0;i<m_max;i++){
                m_ptr=&m_list[i];
		if (m_ptr->r_idx == r_idx) {
		    if (inarea(wpos, &m_ptr->wpos)) {
			already_on_level = TRUE;
			break;
		    }
		}
	}
/* END of ugly hack */

#ifdef DEBUG1
if (r_idx == DEBUG1_IDX) s_printf("DEBUG1: 3\n");
#endif

	/* Hack -- "unique" monsters must be "unique" */
	if ((r_ptr->flags1 & RF1_UNIQUE) &&
	    ((!allow_unique_level(r_idx, wpos)) || (r_ptr->cur_num >= r_ptr->max_num) ||
	    already_on_level))
	{
		/* Cannot create */
		return (FALSE);
	}
#ifdef DEBUG1
if (r_idx == DEBUG1_IDX) s_printf("DEBUG1: 4\n");
#endif

	/* Depth monsters may NOT be created out of depth */
	if ((r_ptr->flags1 & RF1_FORCE_DEPTH) && (getlevel(wpos) < r_ptr->level))
	{
		/* Cannot create */
		return (FALSE);
	}
#ifdef DEBUG1
if (r_idx == DEBUG1_IDX) s_printf("DEBUG1: 5\n");
#endif

        /* Ego Uniques are NOT to be created */
        if ((r_ptr->flags1 & RF1_UNIQUE) && (ego || randuni)) return 0;

	/* If the monster is unique and all players on this level already killed
	   the monster, don't spawn it. (For Morgoth, especially) -C. Blue */
	if (r_ptr->flags1 & RF1_UNIQUE)
	{
		int on_level = 0, who_killed = 0;
		int admin_on_level = 0, admin_who_killed = 0;
		for (i = 1; i <= NumPlayers; i++)
		{
			/* Skip the dungeon master - note:
			   this will provide maximum security against summoning flaws vs players,
			   on the other hand this will prevent the DM from summoning uniques!
			   (Solution: a master_summon flag should be added to this summon routine.) */
//			if (Players[i]->admin_dm) continue;

			/* Count how many players are here */
			if (inarea(&Players[i]->wpos, wpos))
			{
				if (Players[i]->admin_dm) admin_on_level++;
				else on_level++;

				/* Count how many of them have killed this unique monster */
				if (Players[i]->r_killed[r_idx]) {
					if (Players[i]->admin_dm) admin_who_killed++;
					else who_killed++;
				}
			}
		}
		/* If all of them already killed it it must not be spawned */
		/* If players are on the level, they exclusively determine the unique summonability. */
		if ((on_level > 0) && (on_level <= who_killed)) return(FALSE); /* should be '==', but for now lets be tolerant */
		/* If only admins are on the level, allow unique creation
		   if it fits the admin's unique mask */
		if ((on_level == 0) && (admin_on_level <= admin_who_killed)) return(FALSE);
	}
#ifdef DEBUG1
if (r_idx == DEBUG1_IDX) s_printf("DEBUG1: 6\n");
#endif

} /* summon_override_checks */

        /* Now could we generate an Ego Monster */
        r_ptr = race_info_idx(r_idx, ego, randuni);



#if 0
	/* Powerful monster */
	if (r_ptr->level > getlevel(wpos))
	{
		/* Unique monsters */
		if (r_ptr->flags1 & RF1_UNIQUE)
		{
			/* Message for cheaters */
			/*if (cheat_hear) msg_format("Deep Unique (%s).", name);*/
		}

		/* Normal monsters */
		else
		{
			/* Message for cheaters */
			/*if (cheat_hear) msg_format("Deep Monster (%s).", name);*/
		}
	}
	/* Note the monster */
	else if (r_ptr->flags1 & RF1_UNIQUE)
	{
		/* Unique monsters induce message */
		/*if (cheat_hear) msg_format("Unique (%s).", name);*/
	}
#endif	// 0


	/* Access the location */
	c_ptr = &zcave[y][x];

#if 0
	/* XXX makeshift; to be replaced with more generic function */
//	if((r_ptr->flags7 & RF7_AQUATIC) && c_ptr->feat!=FEAT_WATER) return FALSE;
	if(c_ptr->feat == FEAT_DEEP_WATER)
	{
		if (!(r_ptr->flags3 & RF3_UNDEAD) &&
			!(r_ptr->flags7 & (RF7_AQUATIC | RF7_CAN_SWIM | RF7_CAN_FLY)))
			return FALSE;
	}
	else if(r_ptr->flags7 & RF7_AQUATIC) return FALSE;
#endif	// 0

/* override all validity checks! (for summoning done by dungeon master) */
if (!summon_override_checks) {

	/* This usually shouldn't happen */
	/* but can happen when monsters group */
	if (!monster_can_cross_terrain(c_ptr->feat, r_ptr))
	{
#if DEBUG_LEVEL > 2
		s_printf("WARNING: Refused monster: cannot cross terrain\n");
#endif	// DEBUG_LEVEL
		return FALSE;
	}

} /* summon_override_checks */

#ifdef DEBUG1
if (r_idx == DEBUG1_IDX) s_printf("DEBUG1: 7\n");
#endif

	/* Make a new monster */
	c_ptr->m_idx = m_pop();

	/* Mega-Hack -- catch "failure" */
	if (!c_ptr->m_idx) return (FALSE);
#ifdef DEBUG1
if (r_idx == DEBUG1_IDX) s_printf("DEBUG1: 8\n");
#endif
	/* Get a new monster record */
	m_ptr = &m_list[c_ptr->m_idx];

	/* Save the race */
	m_ptr->r_idx = r_idx;
#ifdef RANDUNIS
	m_ptr->ego = ego;
//	m_ptr->name3 = randuni;		
#endif	// RANDUNIS

	/* Place the monster at the location */
	m_ptr->fy = y;
	m_ptr->fx = x;
	wpcopy(&m_ptr->wpos, wpos);

	m_ptr->special = FALSE;

	/* Hack -- Count the monsters on the level */
	r_ptr->cur_num++;


	/* Hack -- count the number of "reproducers" */
	if (r_ptr->flags7 & RF7_MULTIPLY) num_repro++;


	/* Assign maximal hitpoints */
	if (r_ptr->flags1 & RF1_FORCE_MAXHP)
	{
		m_ptr->maxhp = maxroll(r_ptr->hdice, r_ptr->hside);
	}
	else
	{
		m_ptr->maxhp = damroll(r_ptr->hdice, r_ptr->hside);
	}

	/* And start out fully healthy */
	m_ptr->hp = m_ptr->maxhp;


	/* Extract the monster base speed */
	m_ptr->speed = r_ptr->speed;
	m_ptr->mspeed = m_ptr->speed;

	/* Extract base ac and  other things */
	m_ptr->ac = r_ptr->ac;

	for (j = 0; j < 4; j++)
	{
		m_ptr->blow[j].effect = r_ptr->blow[j].effect;
		m_ptr->blow[j].method = r_ptr->blow[j].method;
		m_ptr->blow[j].d_dice = r_ptr->blow[j].d_dice;
		m_ptr->blow[j].d_side = r_ptr->blow[j].d_side;
	}
	m_ptr->level = r_ptr->level;
	m_ptr->exp = MONSTER_EXP(m_ptr->level);
	m_ptr->owner = 0;

	/* Hack -- small racial variety */
	if (!(r_ptr->flags1 & RF1_UNIQUE))
	{
		/* Allow some small variation per monster */
		i = extract_energy[m_ptr->speed] / 10;
		if (i)
		{
			j = rand_spread(0, i);
			m_ptr->mspeed += j;

			if (m_ptr->mspeed < j) m_ptr->mspeed = 255;
		}

//		if (i) m_ptr->mspeed += rand_spread(0, i);
	}


#if 0
	/* Give a random starting energy */
	m_ptr->energy = rand_int(100);
#else /* make nether realm summons less deadly (hounds) */
//	m_ptr->energy = -rand_int(level_speed(wpos) * 2 - 750);
	m_ptr->energy = -rand_int(level_speed(wpos) - 375);
#endif

	/* Hack -- Reduce risk of "instant death by breath weapons" */
	if (r_ptr->flags1 & RF1_FORCE_SLEEP)
	{
		/* Start out with minimal energy */
		m_ptr->energy = rand_int(10);
	}


	/* No "damage" yet */
	m_ptr->stunned = 0;
	m_ptr->confused = 0;
	m_ptr->monfear = 0;

	/* No knowledge */
	m_ptr->cdis = 0;

	/* clone value */
	m_ptr->clone = clo;
	/* does this monster summon clones yet? - C. Blue */
	m_ptr->clone_summoning = clone_summoning;
	if (cfg.clone_summoning == 999)
		m_ptr->clone_summoning = 0;
	else if (m_ptr->clone_summoning > cfg.clone_summoning)
		m_ptr->clone += 25 * (m_ptr->clone_summoning - cfg.clone_summoning);
	if (m_ptr->clone > 100) m_ptr->clone = 100;
	/* Don't let it overflow over time.. (well, not very realistic) */
	if (m_ptr->clone_summoning - cfg.clone_summoning > 4) m_ptr->clone_summoning = 4 + cfg.clone_summoning;
    
	/* Hack - Unique monsters are never clones - C. Blue */
	if (r_ptr->flags1 & RF1_UNIQUE)
		/* hack for global event "Arena Monster Challenge" - C. Blue */
		if (clone_summoning != 1000) m_ptr->clone = 0;

	for (Ind = 1; Ind < NumPlayers + 1; Ind++)
	{
		if (Players[Ind]->conn == NOT_CONNECTED)
			continue;

		Players[Ind]->mon_los[c_ptr->m_idx] = FALSE;
		Players[Ind]->mon_vis[c_ptr->m_idx] = FALSE;
	}

#if 0	// may I change it somewhat?	- Jir -
	/* Should we gain levels ? */
	if (getlevel(wpos) >100)
	{
		int l = m_ptr->level + 100;

		m_ptr->exp = MONSTER_EXP(l);
		monster_check_experience(c_ptr->m_idx, TRUE);
	}
#endif //no thx
#if 0	// 0
	/* Should we gain levels ? */
	/* Starting from 2600ft, 'too weak' monsters gain levels. */
	while (getlevel(wpos) > MONSTER_TOO_WEAK + m_ptr->level)
	{
		int l = m_ptr->level + MONSTER_TOO_WEAK;

		m_ptr->exp = MONSTER_EXP(l);
		monster_check_experience(c_ptr->m_idx, TRUE);
	}
#endif	// 0

	if (getlevel(wpos) > (m_ptr->level + 7)) {
	    int l = m_ptr->level + ((getlevel(wpos) - (m_ptr->level + 7)) / 3);
	    m_ptr->exp = MONSTER_EXP(l);
	    monster_check_experience(c_ptr->m_idx, TRUE);
	}

	strcpy(buf, (r_name + r_ptr->name));

	/* Update the monster */
	update_mon(c_ptr->m_idx, TRUE);


	/* Assume no sleeping */
	m_ptr->csleep = 0;

	/* Enforce sleeping if needed */
	if (slp && r_ptr->sleep)
	{
		int val = r_ptr->sleep;
		m_ptr->csleep = ((val * 2) + randint(val * 10));
	}

/*	if(m_ptr->hold_o_idx){
		s_printf("AHA! monster created with an object in hand!\n");
		m_ptr->hold_o_idx=0;
	}*/

	/* Remember this monster's starting values in case they get temporarily decreased (by traps!) */
	/* STR */
	for (j = 0; j < 4; j++) {
		m_ptr->blow[j].org_d_dice = r_ptr->blow[j].d_dice;
		m_ptr->blow[j].org_d_side = r_ptr->blow[j].d_side;
	}
	/* DEX */
	m_ptr->org_ac = m_ptr->ac;
	/* CON */
	m_ptr->org_maxhp = m_ptr->maxhp;

	/* Success */
	/* Report some very interesting monster creating: */
	if (r_idx == 860) s_printf("Sauron was created on %d\n", getlevel(wpos));
#ifdef ENABLE_DIVINE
	if (r_idx == 1104) s_printf("Candlebearer was created on %d\n", getlevel(wpos));
	if (r_idx == 1105) s_printf("Darkling was created on %d\n", getlevel(wpos));
#endif
	if (r_idx == 862) {
		dun_level *l_ptr;
		s_printf("Morgoth was created on %d\n", getlevel(wpos));
#ifdef MORGOTH_GHOST_DEATH_LEVEL
		l_ptr = getfloor(wpos);
		l_ptr->flags1 |= LF1_NO_GHOST;
#endif
#ifdef MORGOTH_DANGEROUS_LEVEL
		/* make it even harder? */
		l_ptr->flags1 |= (LF1_NO_GENO | LF1_NO_DESTROY);
#endif
		/* Page all dungeon masters to notify them of a possible Morgoth-fight >:) - C. Blue */
		if (watch_morgoth)
		for (Ind = 1; Ind <= NumPlayers; Ind++)
		{
			if (Players[Ind]->conn == NOT_CONNECTED)
				continue;
			if (Players[Ind]->admin_dm && !(Players[Ind]->afk && !streq(Players[Ind]->afk_msg, "watch"))) Players[Ind]->paging = 4;
		}
		/* if it was a live spawn, adjust his power according to amount of players on his floor */
		if (!cave_set_quietly) check_Morgoth();
	}
	if (r_idx == 1032) s_printf("Tik'Svrzllat was created on %d\n", getlevel(wpos));
	if (r_idx == 1067) s_printf("The Hellraiser was created on %d\n", getlevel(wpos));
	if (r_idx == 1085) s_printf("Dor was created on %d\n", getlevel(wpos));
	/* no easy escape from Zu-Aon besides resigning by recalling! */
	if (r_idx == 1097) {
		dun_level *l_ptr;
		s_printf("Zu-Aon, The Cosmic Border Guard was created on %d\n", getlevel(wpos));
		l_ptr = getfloor(wpos);
		l_ptr->flags1 |= (LF1_NO_GENO | LF1_NO_DESTROY);
	}

summon_override_checks = 0; /* reset admin summoning override flags */

	return (TRUE);
}

/*
 * Maximum size of a group of monsters
 */
#define GROUP_MAX       32 /* 20 would be nice for hounds maybe - C. Blue */


/*
 * Attempt to place a "group" of monsters around the given location
 */
static bool place_monster_group(struct worldpos *wpos, int y, int x, int r_idx, bool slp, bool small, int s_clone, int clone_summoning)
{
	monster_race *r_ptr = &r_info[r_idx];

	int n, i;
	int total = 0, extra = 0;

	int hack_n = 0;

	byte hack_y[GROUP_MAX];
	byte hack_x[GROUP_MAX];
	cave_type **zcave;
	if(!(zcave=getcave(wpos))) return(FALSE);

	/* Pick a group size */
	total = randint(13);

	/* Hard monsters, small groups */
	if (r_ptr->level > getlevel(wpos))
	{
		extra = r_ptr->level - getlevel(wpos);
		extra = 0 - randint(extra);
	}

	/* Easy monsters, large groups */
	else if (r_ptr->level < getlevel(wpos))
	{
		extra = getlevel(wpos) - r_ptr->level;
		extra = randint(extra);
	}

	/* Hack -- limit group reduction */
	if (extra > 12) extra = 12;

	/* Modify the group size */
	total += extra;

	/* fewer friends.. */
	if (small) total >>= 2;

	/* Minimum size */
	if (total < 1) total = 1;

	/* Maximum size */
	if (total > GROUP_MAX) total = GROUP_MAX;

	/* Start on the monster */
	hack_n = 1;
	hack_x[0] = x;
	hack_y[0] = y;

	/* Puddle monsters, breadth first, up to total */
	for (n = 0; (n < hack_n) && (hack_n < total); n++)
	{
		/* Grab the location */
		int hx = hack_x[n];
		int hy = hack_y[n];

		/* Check each direction, up to total */
		for (i = 0; (i < 8) && (hack_n < total); i++)
		{
			int mx = hx + ddx_ddd[i];
			int my = hy + ddy_ddd[i];

			/* Walls and Monsters block flow */
			/* Commented out for summoning on mountains */
#if 1
			if (!cave_empty_bold(zcave, my, mx)) continue;
#endif
			/* Attempt to place another monster */
			if (place_monster_one(wpos, my, mx, r_idx, pick_ego_monster(r_idx, getlevel(wpos)), 0, slp, s_clone, clone_summoning))
			{
				/* Add it to the "hack" set */
				hack_y[hack_n] = my;
				hack_x[hack_n] = mx;
				hack_n++;
			}
		}
	}

	/* Success */
	return (TRUE);
}


/*
 * Hack -- help pick an escort type
 */
static int place_monster_idx = 0;

/*
 * Hack -- help pick an escort type
 */
static bool place_monster_okay_escort(int r_idx)
{
	monster_race *r_ptr = &r_info[place_monster_idx];

	monster_race *z_ptr = &r_info[r_idx];

	/* Require similar "race" */
	if (z_ptr->d_char != r_ptr->d_char) return (FALSE);

	/* Skip more advanced monsters */
	if (z_ptr->level > r_ptr->level) return (FALSE);

	/* Skip Black Dogs */
	if (z_ptr->flags0 & RF0_NO_GROUP_MASK) return (FALSE);

	/* Skip unique monsters */
	if (z_ptr->flags1 & RF1_UNIQUE) return (FALSE);

	/* Paranoia -- Skip identical monsters */
	if (place_monster_idx == r_idx) return (FALSE);

	/* Okay */
	return (TRUE);
}


/*
 * Attempt to place a monster of the given race at the given location
 *
 * Note that certain monsters are now marked as requiring "friends".
 * These monsters, if successfully placed, and if the "grp" parameter
 * is TRUE, will be surrounded by a "group" of identical monsters.
 *
 * Note that certain monsters are now marked as requiring an "escort",
 * which is a collection of monsters with similar "race" but lower level.
 *
 * Some monsters induce a fake "group" flag on their escorts.
 *
 * Note the "bizarre" use of non-recursion to prevent annoying output
 * when running a code profiler.
 *
 * Note the use of the new "monster allocation table" code to restrict
 * the "get_mon_num()" function to "legal" escort types.
 */
bool place_monster_aux(struct worldpos *wpos, int y, int x, int r_idx, bool slp, bool grp, int clo, int clone_summoning)
{
	int                     i;

	monster_race    *r_ptr = &r_info[r_idx];

	cave_type **zcave;
	int level=getlevel(wpos);
	if(!(zcave=getcave(wpos))) return(FALSE);

#ifdef ARCADE_SERVER
	if(wpos->wx == cfg.town_x && wpos->wy == cfg.town_y && wpos->wz > 0) return(FALSE);
#endif

if (!summon_override_checks) {
	/* Do not allow breeders to spawn in the wilderness - the_sandman */
	if ((r_ptr->flags7 & RF7_MULTIPLY) && !(wpos->wz)) return (FALSE);
}
	/* Place one monster, or fail */
	if (!place_monster_one(wpos, y, x, r_idx, pick_ego_monster(r_idx, level), 0, slp, clo, clone_summoning)) return (FALSE);

	/* Require the "group" flag */
	if (!grp) return (TRUE);

	/* Friend for certain monsters */
	if (r_ptr->flags1 & RF1_FRIEND)
	{
		/* Attempt to place a group */
		(void)place_monster_group(wpos, y, x, r_idx, slp, TRUE, clo, clone_summoning);
	}

	/* Friends for certain monsters */
	if (r_ptr->flags1 & RF1_FRIENDS)
	{
		/* Attempt to place a group */
		(void)place_monster_group(wpos, y, x, r_idx, slp, FALSE, clo, clone_summoning);
	}


	/* Escorts for certain monsters */
	if (r_ptr->flags1 & RF1_ESCORT)
	{
		dungeon_type *dt_ptr = getdungeon(wpos);
		int dun_type = 0;
		if (dt_ptr) dun_type = dt_ptr->type;

		/* Try to place several "escorts" */
		for (i = 0; i < 50; i++)
		{
			int nx, ny, z, d = 3;

			/* Pick a location */
			scatter(wpos, &ny, &nx, y, x, d, 0);
			/* Require empty grids */
			if (!cave_empty_bold(zcave, ny, nx)) continue;

			/* Set the escort index */
			place_monster_idx = r_idx;

			/* Set the escort hook */
			get_mon_num_hook = place_monster_okay_escort;

			/* Prepare allocation table */
			get_mon_num_prep();


			/* Pick a random race */
			z = get_mon_num(r_ptr->level, dun_type);


			/* Remove restriction */
			get_mon_num_hook = dungeon_aux;

			/* Prepare allocation table */
			get_mon_num_prep();


			/* Handle failure */
			if (!z) break;

			/* Place a single escort */
			(void)place_monster_one(wpos, ny, nx, z, pick_ego_monster(z, level), 0, slp, clo, clone_summoning);

			/* Place a "group" of escorts if needed */
			if ((r_info[z].flags1 & RF1_FRIENDS) ||
			    (r_ptr->flags1 & RF1_ESCORTS))
			{
				/* Place a group of monsters */
				(void)place_monster_group(wpos, ny, nx, z, slp, FALSE, clo, clone_summoning);
			}
		}
	}


	/* Success */
	return (TRUE);
}


/*
 * Hack -- attempt to place a monster at the given location
 *
 * Attempt to find a monster appropriate to the "monster_level"
 */
bool place_monster(struct worldpos *wpos, int y, int x, bool slp, bool grp)
{
	int r_idx;
#ifdef HALLOWEEN
	int l = getlevel(wpos);
#endif
	struct dungeon_type *d_ptr;

	d_ptr=getdungeon(wpos);


#ifdef HALLOWEEN
	/* Place a Great Pumpkin sometimes -- WARNING: HARDCODED r_idx */
 #ifndef RPG_SERVER
	if ((great_pumpkin_timer == 0) && (l < 40) && (wpos->wz != 0)) {
  #if 0
		r_idx = 1086 + rand_int(2);
  #else
		r_idx = 1088;//3k HP, smallest version
		if (magik(25)) r_idx = 1087; /* sometimes tougher */
		if (l > 10) r_idx = 1087;//6k HP
		if (magik(25)) r_idx = 1086; /* sometimes tougher */
		if (l > 20) r_idx = 1086;//10k HP
  #endif
 #else
	if ((great_pumpkin_timer == 0) && (l < 50) && (wpos->wz != 0) &&
	    !(d_ptr->flags2 & DF2_NO_DEATH)) { /* not in Training Tower */
  #if 0
		r_idx = 1086 + rand_int(2);
  #else
		r_idx = 1088;//3k HP, smallest version
		if (magik(15)) r_idx = 1087; /* sometimes tougher */
		if (l > 15) r_idx = 1087;//6k HP
		if (magik(15)) r_idx = 1086; /* sometimes tougher */
		if (l > 30) r_idx = 1086;//10k HP
  #endif
 #endif
		if (place_monster_aux(wpos, y, x, r_idx, FALSE, FALSE, 0, 0)) {
//			great_pumpkin_timer = 15 + rand_int(45);  <- now done in monster_death() ! So no more duplicate pumpkins.
			/* log */
			s_printf("%s Generated Great Pumpkin (%d) on %d,%d,%d (lev %d)\n", showtime(), r_idx, wpos->wx, wpos->wy, wpos->wz, l);
			great_pumpkin_timer = -1; /* put generation on hold */
			check_Pumpkin(); /* recall high-level players off this floor! */
			return (TRUE);
		}
		/* oupsee */
//		great_pumpkin_timer = 1; /* <- just paranoia: no mass-emptiness in case above always fails for unknown reasons */
		return (FALSE);
	}
#endif


	/* Specific filter - should be made more useful */
	/* Ok, I'll see to that later	- Jir - */
	 
	if(d_ptr && (d_ptr->r_char[0] || d_ptr->nr_char[0])){
		int i,j=0;
		monster_race *r_ptr;
		while((r_idx=get_mon_num(monster_level, 0))){
			if(j++>250) break;
			r_ptr=&r_info[r_idx];
			if(d_ptr->r_char[0]){
                		for (i = 0; i < 10; i++)
                		{
                        		if (r_ptr->d_char == d_ptr->r_char[i]) break;
                		}
				if(i<10) break;
				continue;
			}
			if(d_ptr->nr_char[0]){
                		for (i = 0; i < 10; i++)
                		{
                        		if (r_ptr->d_char == d_ptr->r_char[i]) continue;
                		}
				break;
			}
		}
	}
	else
	{
		dungeon_type *dt_ptr = getdungeon(wpos);
		int dun_type = 0;
		if (dt_ptr) dun_type = dt_ptr->type;

		/* Set monster restriction */
		set_mon_num2_hook(wpos, y, x);

		/* Prepare allocation table */
		get_mon_num_prep();

		/* Pick a monster */
		r_idx = get_mon_num(monster_level, dun_type);

		/* Reset restriction */
		get_mon_num2_hook = NULL;

		/* Prepare allocation table */
		get_mon_num_prep();
	}

	/* Handle failure */
	if (!r_idx) return (FALSE);

	/* Attempt to place the monster */
	if (place_monster_aux(wpos, y, x, r_idx, slp, grp, FALSE, 0)) return (TRUE);

	/* Oops */
	return (FALSE);
}

/*
 * XXX XXX XXX Player Ghosts are such a hack, they have been completely
 * removed until Angband 2.8.0, in which there will actually be a small
 * number of "unique" monsters which will serve as the "player ghosts".
 * Each will have a place holder for the "name" of a deceased player,
 * which will be extracted from a "bone" file, or replaced with a
 * "default" name if a real name is not available.  Each ghost will
 * appear exactly once and will not induce a special feeling.
 *
 * Possible methods:
 *   (s) 1 Skeleton
 *   (z) 1 Zombie
 *   (M) 1 Mummy
 *   (G) 1 Polterguiest, 1 Spirit, 1 Ghost, 1 Shadow, 1 Phantom
 *   (W) 1 Wraith
 *   (V) 1 Vampire, 1 Vampire Lord
 *   (L) 1 Lich
 *
 * Possible change: Lose 1 ghost, Add "Master Lich"
 *
 * Possible change: Lose 2 ghosts, Add "Wraith", Add "Master Lich"
 *
 * Possible change: Lose 4 ghosts, lose 1 vampire lord
 *
 * Note that ghosts should never sleep, should be very attentive, should
 * have maximal hitpoints, drop only good (or great) items, should be
 * cold blooded, evil, undead, immune to poison, sleep, confusion, fear.
 *
 * Base monsters:
 *   Skeleton
 *   Zombie
 *   Mummy
 *   Poltergeist
 *   Spirit
 *   Ghost
 *   Vampire
 *   Wraith
 *   Vampire Lord
 *   Shadow
 *   Phantom
 *   Lich
 *
 * This routine will simply extract ghost names from files, and
 * attempt to allocate a player ghost somewhere in the dungeon,
 * note that normal allocation may also attempt to place ghosts,
 * so we must work with some form of default names.
 *
 * XXX XXX XXX XXX
 */





/*
 * Attempt to allocate a random monster in the dungeon.
 *
 * Place the monster at least "dis" distance from the player.
 *
 * Use "slp" to choose the initial "sleep" status
 *
 * Use "monster_level" for the monster level
 */
bool alloc_monster(struct worldpos *wpos, int dis, int slp)
{
	int                     y, x, i, d, min_dis = 999;
	int                     tries = 0;
	player_type *p_ptr;
	cave_type **zcave;
	if(!(zcave=getcave(wpos))) return(FALSE);

	/* Find a legal, distant, unoccupied, space */
	while (tries < 50)
	{
		/* Increase the counter */
		tries++;

		/* Pick a location */
		y = rand_int(getlevel(wpos) ? MAX_HGT : MAX_HGT);
		x = rand_int(getlevel(wpos) ? MAX_WID : MAX_WID);
		
		/* Require "naked" floor grid */
		if (!cave_naked_bold(zcave, y, x)) continue;

		/* Accept far away grids */
		for (i = 1; i < NumPlayers + 1; i++)
		{
			p_ptr = Players[i];

			/* Skip him if he's not playing */
			if (p_ptr->conn == NOT_CONNECTED)
				continue;

			/* Skip him if he's not on this depth */
			if(!inarea(wpos, &p_ptr->wpos))
				continue;

			if ((d = distance(y, x, p_ptr->py, p_ptr->px)) < min_dis)
				min_dis = d;
		}

		if (min_dis >= dis)
			break;
	}

	/* Abort */
	if (tries >= 50)
		return (FALSE);

	/*printf("Trying to place a monster at %d, %d.\n", y, x);*/

	/* Attempt to place the monster, allow groups */
	if (place_monster(wpos, y, x, slp, TRUE)) return (TRUE);

	/* Nope */
	return (FALSE);
}

/*
 * Hack -- the "type" of the current "summon specific"
 */
static int summon_specific_type = 0;


/*
 * Hack -- help decide if a monster race is "okay" to summon
 */
static bool summon_specific_okay(int r_idx)
{
	monster_race *r_ptr = &r_info[r_idx];

	bool okay = FALSE;


	/* Hack -- no specific type specified */
	if (!summon_specific_type) return (TRUE);


	/* Check our requirements */
	switch (summon_specific_type)
	{
		case SUMMON_ANT:
		{
			okay = ((r_ptr->d_char == 'a') &&
				!(r_ptr->flags1 & RF1_UNIQUE));
			break;
		}

		case SUMMON_SPIDER:
		{
			okay = ((r_ptr->d_char == 'S') &&
				!(r_ptr->flags1 & RF1_UNIQUE));
			break;
		}

		case SUMMON_HOUND:
		{
			okay = (((r_ptr->d_char == 'C') || (r_ptr->d_char == 'Z')) &&
				!(r_ptr->flags1 & RF1_UNIQUE));
			break;
		}

		case SUMMON_HYDRA:
		{
			okay = ((r_ptr->d_char == 'M') &&
				!(r_ptr->flags1 & RF1_UNIQUE));
			break;
		}

		case SUMMON_ANGEL:
		{
			okay = ((r_ptr->d_char == 'A') &&
				!(r_ptr->flags1 & RF1_UNIQUE));
			break;
		}

		case SUMMON_DEMON:
		{
			okay = ((r_ptr->flags3 & RF3_DEMON) &&
				!(r_ptr->flags1 & RF1_UNIQUE));
			break;
		}

		case SUMMON_UNDEAD:
		{
			okay = ((r_ptr->flags3 & RF3_UNDEAD) &&
				!(r_ptr->flags1 & RF1_UNIQUE));
			break;
		}

		case SUMMON_DRAGON:
		{
			okay = ((r_ptr->flags3 & RF3_DRAGON) &&
				!(r_ptr->flags1 & RF1_UNIQUE));
			break;
		}

		case SUMMON_HI_UNDEAD:
		{
			okay = (((r_ptr->d_char == 'L') ||
				(r_ptr->d_char == 'V') ||
				(r_ptr->d_char == 'W')) &&
				(r_ptr->level >= 45));
			break;
		}

		case SUMMON_HI_DRAGON:
		{
			okay = (r_ptr->d_char == 'D');
			break;
		}

		case SUMMON_WRAITH:
		{
			okay = ((r_ptr->d_char == 'W') &&
				(r_ptr->flags1 & RF1_UNIQUE));
			break;
		}

		case SUMMON_UNIQUE:
		{
			okay = (r_ptr->flags1 & RF1_UNIQUE);
			break;
		}

		/* PernA-addition
		 * many of them are unused for now.	- Jir -
		 * */
		case SUMMON_BIZARRE1:
		{
			okay = ((r_ptr->d_char == 'm') &&
			       !(r_ptr->flags1 & (RF1_UNIQUE)));
			break;
		}
		case SUMMON_BIZARRE2:
		{
			okay = ((r_ptr->d_char == 'b') &&
			       !(r_ptr->flags1 & (RF1_UNIQUE)));
			break;
		}
		case SUMMON_BIZARRE3:
		{
			okay = ((r_ptr->d_char == 'Q') &&
			       !(r_ptr->flags1 & (RF1_UNIQUE)));
			break;
		}

		case SUMMON_BIZARRE4:
		{
			okay = ((r_ptr->d_char == 'v') &&
			       !(r_ptr->flags1 & (RF1_UNIQUE)));
			break;
		}

		case SUMMON_BIZARRE5:
		{
			okay = ((r_ptr->d_char == '$') &&
			       !(r_ptr->flags1 & (RF1_UNIQUE)));
			break;
		}

		case SUMMON_BIZARRE6:
		{
			okay = (((r_ptr->d_char == '!') ||
			         (r_ptr->d_char == '?') ||
			         (r_ptr->d_char == '=') ||
			         (r_ptr->d_char == '$') ||
			         (r_ptr->d_char == '|') ||
					 (r_ptr->flags9 & (RF9_MIMIC)) ) &&
			        !(r_ptr->flags1 & (RF1_UNIQUE)));
			break;
		}

                case SUMMON_HI_DEMON:
		{
			okay = ((r_ptr->flags3 & (RF3_DEMON)) &&
                                (r_ptr->d_char == 'U') &&
				(r_ptr->level >= 49) &&
			       !(r_ptr->flags1 & RF1_UNIQUE));
			break;
		}

#if 1	// welcome, Julian Lighton Hack ;)
		case SUMMON_KIN:
		{
			okay = ((r_ptr->d_char == summon_kin_type) &&
			       !(r_ptr->flags1 & (RF1_UNIQUE)));
			break;
		}
#endif	// 0

		case SUMMON_DAWN:
		{
			okay = ((strstr((r_name + r_ptr->name),"the Dawn")) &&
			       !(r_ptr->flags1 & (RF1_UNIQUE)));
			break;
		}

		case SUMMON_ANIMAL:
		{
			okay = ((r_ptr->flags3 & (RF3_ANIMAL)) &&
			       !(r_ptr->flags1 & (RF1_UNIQUE)));
			break;
		}

		case SUMMON_ANIMAL_RANGER:
		{
			okay = ((r_ptr->flags3 & (RF3_ANIMAL)) &&
			       (strchr("abcflqrwBCIJKMRS", r_ptr->d_char)) &&
			       !(r_ptr->flags3 & (RF3_DRAGON))&&
			       !(r_ptr->flags3 & (RF3_EVIL)) &&
			       !(r_ptr->flags3 & (RF3_UNDEAD))&&
			       !(r_ptr->flags3 & (RF3_DEMON)) &&
			       !(r_ptr->flags4 || r_ptr->flags5 || r_ptr->flags6) &&
			       !(r_ptr->flags1 & (RF1_UNIQUE)));
			break;
		}

		case SUMMON_HI_UNDEAD_NO_UNIQUES:
		{
			okay = (((r_ptr->d_char == 'L') ||
			         (r_ptr->d_char == 'V') ||
			         (r_ptr->d_char == 'W')) &&
				 (r_ptr->level >= 45) &&
			        !(r_ptr->flags1 & (RF1_UNIQUE)));
			break;
		}

		case SUMMON_HI_DRAGON_NO_UNIQUES:
		{
			okay = ((r_ptr->d_char == 'D') &&
			       !(r_ptr->flags1 &(RF1_UNIQUE)));
			break;
		}

		case SUMMON_NO_UNIQUES:
		{
			okay = (!(r_ptr->flags1 & (RF1_UNIQUE)));
			break;
		}

		case SUMMON_PHANTOM:
		{
			okay = ((strstr((r_name + r_ptr->name),"Phantom")) &&
			       !(r_ptr->flags1 & (RF1_UNIQUE)));
			break;
		}

		case SUMMON_ELEMENTAL:
		{
			okay = ((strstr((r_name + r_ptr->name),"lemental")) &&
			       !(r_ptr->flags1 & (RF1_UNIQUE)));
			break;
		}
                case SUMMON_DRAGONRIDER:
		{
                        okay = (r_ptr->flags3 & RF3_DRAGONRIDER)?TRUE:FALSE;
			break;
		}

		case SUMMON_BLUE_HORROR:
		{
			okay = ((strstr((r_name + r_ptr->name),"lue horror")) &&
			       !(r_ptr->flags1 & (RF1_UNIQUE)));
			break;
		}

                case SUMMON_BUG:
		{
                        okay = ((strstr((r_name + r_ptr->name),"Software bug")) &&
			       !(r_ptr->flags1 & (RF1_UNIQUE)));
			break;
		}

                case SUMMON_RNG:
		{
                        okay = ((strstr((r_name + r_ptr->name),"Random Number Generator")) &&
			       !(r_ptr->flags1 & (RF1_UNIQUE)));
			break;
		}
                case SUMMON_MINE:
		{
                        okay = (r_ptr->flags1 & RF1_NEVER_MOVE)?TRUE:FALSE;
			break;
		}

                case SUMMON_HUMAN:
		{
                        okay = ((r_ptr->d_char == 'p') &&
			        !(r_ptr->flags1 & (RF1_UNIQUE)));
			break;
		}

		case SUMMON_SHADOWS:
		{
			okay = ((r_ptr->d_char == 'G') &&
			        !(r_ptr->flags1 & (RF1_UNIQUE)));
			break;
		}

		case SUMMON_QUYLTHULG:
		{
			okay = ((r_ptr->d_char == 'Q') &&
				!(r_ptr->flags1 & (RF1_UNIQUE)));
			break;
		}

		case SUMMON_VERMIN:
		{
			okay = (r_ptr->flags7 & RF7_MULTIPLY)?TRUE:FALSE;
			break;
		}

		case SUMMON_IMMOBILE:
		{
			okay = (r_ptr->flags1 & RF1_NEVER_MOVE) &&
				!(r_ptr->flags4 && r_ptr->flags5 && r_ptr->flags6);
			break;
		}

#ifdef USE_LUA
#if 0	// let's leave it to DG :)
		case SUMMON_LUA:
		{
			okay = summon_lua_okay(r_idx);
			break;
		}
#endif
#endif

		case SUMMON_HI_MONSTER:
		case SUMMON_HI_MONSTERS:
		{
			okay = (r_ptr->level >= 60);
			break;
		}

		case SUMMON_HI_UNIQUE:
		{
			okay = ((r_ptr->flags1 & RF1_UNIQUE) && (r_ptr->level >= 60));
			break;
		}

	}

	/* Result */
	return (okay);
}


/*
 * Place a monster (of the specified "type") near the given
 * location.  Return TRUE iff a monster was actually summoned.
 *
 * We will attempt to place the monster up to 10 times before giving up.
 *
 * Note: SUMMON_UNIQUE and SUMMON_WRAITH (XXX) will summon Unique's
 * Note: SUMMON_HI_UNDEAD and SUMMON_HI_DRAGON may summon Unique's
 * Note: None of the other summon codes will ever summon Unique's.
 *
 * This function has been changed.  We now take the "monster level"
 * of the summoning monster as a parameter, and use that, along with
 * the current dungeon level, to help determine the level of the
 * desired monster.  Note that this is an upper bound, and also
 * tends to "prefer" monsters of that level.  Currently, we use
 * the average of the dungeon and monster levels, and then add
 * five to allow slight increases in monster power.
 *
 * Note that we use the new "monster allocation table" creation code
 * to restrict the "get_mon_num()" function to the set of "legal"
 * monsters, making this function much faster and more reliable.
 *
 * Note that this function may not succeed, though this is very rare.
 */
bool summon_specific(struct worldpos *wpos, int y1, int x1, int lev, int s_clone, int type, int allow_sidekicks, int clone_summoning)
{
	int i, x, y, r_idx;
	cave_type **zcave;
	if(!(zcave=getcave(wpos))) return(FALSE);

	/* Look for a location */
	for (i = 0; i < 20; ++i)
	{
		/* Pick a distance */
		int d = (i / 15) + 1;

		/* Pick a location */
		scatter(wpos, &y, &x, y1, x1, d, 0);

		/* Require "empty" floor grid */
		/* Changed for summoning on mountains */
		if (!cave_empty_bold(zcave, y, x)) continue;
#if 0
	        if (!(cave_empty_bold(zcave, y, x) ||
	            (cave_empty_mountain(zcave, y, x) &&
	            ((r_ptr->flags2 && RF2_PASS_WALL) ||
	             (r_ptr->flags8 && RF8_WILD_MOUNTAIN) ||
	              (r_ptr->flags8 && RF8_WILD_VOLCANO))))) continue;
#endif
		/* Hack -- no summon on glyph of warding */
		if (zcave[y][x].feat == FEAT_GLYPH) continue;

		/* Okay */
		break;
	}

	/* Failure */
	if (i == 20) return (FALSE);


	/* Save the "summon" type */
	summon_specific_type = type;


	/* Require "okay" monsters */
	get_mon_num_hook = summon_specific_okay;

	/* Prepare allocation table */
	get_mon_num_prep();


	/* Pick a monster, using the level calculation */
	/* XXX: Exception for Morgoth (so that he won't summon townies)
	 * This fix presumes Morgie and Morgie only has level 100 */
	
	for (i = 0; i < 10; i++) { /* try a couple of times */
		/* Hack for RF0_S_HI_ flags */
		if (type == SUMMON_HI_MONSTER || type == SUMMON_HI_MONSTERS || type == SUMMON_HI_UNIQUE) {
			/* Ok, now let them summon what they can */
			r_idx = get_mon_num(100 + 5, 0);
			if (r_info[r_idx].level < 60) {
				r_idx = 0; /* failure - see below */
				continue;
			}
		} else {
			/* Ok, now let them summon what they can */
			r_idx = get_mon_num((getlevel(wpos) + lev) / 2 + 5, 0);
		}
		break;
	}
	
	/* Don't allow uniques if escorts/friends (sidekicks) weren't allowed */
	if (!allow_sidekicks && (r_info[r_idx].flags1 & RF1_UNIQUE)) return (FALSE);

	/* Remove restriction */
	get_mon_num_hook = dungeon_aux;

	/* Prepare allocation table */
	get_mon_num_prep();


	/* Handle failure */
	if (!r_idx) return (FALSE);

	/* Attempt to place the monster (awake, allow groups) */
	if (!place_monster_aux(wpos, y, x, r_idx, FALSE, allow_sidekicks?TRUE:FALSE, s_clone, clone_summoning)) return (FALSE);

	/* Success */
	return (TRUE);
}

/* summon a specific race near this location */
/* summon until we can't find a location or we have summoned size */
bool summon_specific_race(struct worldpos *wpos, int y1, int x1, int r_idx, int s_clone, unsigned char size)
{
	int c, i, x, y;
	cave_type **zcave;
	if(!(zcave=getcave(wpos))) return(FALSE);

	/* Handle failure */
	if (!r_idx) return (FALSE);

	/* for each monster we are summoning */

	for (c = 0; c < size; c++)
	{       

		/* Look for a location */
		for (i = 0; i < 200; ++i)
		{
			/* Pick a distance */
			int d = (i / 15) + 1;

			/* Pick a location */
			scatter(wpos, &y, &x, y1, x1, d, 0);

			/* Require "empty" floor grid */
			if (!cave_empty_bold(zcave, y, x)) continue;

			/* Hack -- no summon on glyph of warding */
			if (zcave[y][x].feat == FEAT_GLYPH) continue;

			/* Okay */
			break;
		}

		/* Failure */
		if (i == 20) return (FALSE);

		/* Attempt to place the monster (awake, don't allow groups) */
		if (!place_monster_aux(wpos, y, x, r_idx, FALSE, FALSE, s_clone == 101 ? 100 : s_clone, s_clone == 101 ? 1000 : 0))
			return (FALSE);

	}

	/* Success */
	return (TRUE);
}


/* summon a specific race at a random location */
bool summon_specific_race_somewhere(struct worldpos *wpos, int r_idx, int s_clone, unsigned char size)
{
	int                     y, x;
	int                     tries = 0;

	cave_type **zcave;
	if(!(zcave=getcave(wpos))) return(FALSE);

	/* Find a legal, distant, unoccupied, space */
	while (tries < 50)
	{
		/* Increase the counter */
		tries++;

		/* Pick a location */
		y = rand_int(getlevel(wpos) ? MAX_HGT : MAX_HGT);
		x = rand_int(getlevel(wpos) ? MAX_WID : MAX_WID);

		/* Require "naked" floor grid */
		if (!cave_naked_bold(zcave, y, x)) continue;


		/* Abort */
		if (tries >= 50)
			return (FALSE);

		/* We have a valid location */
		break;
	}

	/* Attempt to place the monster */
	if (summon_specific_race(wpos, y, x, r_idx, s_clone, size)) return TRUE;
	return (FALSE);
}

/* summon a single monster in every detail in a random location */
int summon_detailed_one_somewhere(struct worldpos *wpos, int r_idx, int ego, bool slp, int s_clone)
{
	int                     y, x;
	int                     tries = 0;

	cave_type **zcave;
	if(!(zcave=getcave(wpos))) return(FALSE);

	/* Find a legal, distant, unoccupied, space */
	while (tries < 50)
	{
		/* Increase the counter */
		tries++;

		/* Pick a location */
		y = rand_int(getlevel(wpos) ? MAX_HGT : MAX_HGT);
		x = rand_int(getlevel(wpos) ? MAX_WID : MAX_WID);

		/* Require "naked" floor grid */
		if (!cave_naked_bold(zcave, y, x)) continue;


		/* these two possibly superfluous? */
		/* Require "empty" floor grid */
		if (!cave_empty_bold(zcave, y, x)) continue;
		/* Hack -- no summon on glyph of warding */
		if (zcave[y][x].feat == FEAT_GLYPH) continue;


		/* Abort */
		if (tries >= 50)
			return (FALSE);

		/* We have a valid location */
		break;
	}

	if (!place_monster_one(wpos, y, x, r_idx, ego, FALSE, slp, s_clone == 101 ? 100 : s_clone, s_clone == 101 ? 1000 : 0))
		return (FALSE);

	/* Success */
	return (zcave[y][x].m_idx);
}





/*
 * Let the given monster attempt to reproduce.
 *
 * Note that "reproduction" REQUIRES empty space.
 */
bool multiply_monster(int m_idx)
{
	monster_type	*m_ptr = &m_list[m_idx];
        monster_race    *r_ptr = race_inf(m_ptr);

	int                     i, y, x;

	bool result = FALSE;
	struct worldpos *wpos=&m_ptr->wpos;
	cave_type **zcave;
	if(!(zcave=getcave(wpos))) return(FALSE);
	if(m_ptr->clone>90) return(FALSE);

	/* NO UNIQUES */
	if (r_ptr->flags1 & RF1_UNIQUE) return FALSE;
	/* Not in town -_- */
	if (istown(wpos)) return FALSE;

	/* Try up to 18 times */
	for (i = 0; i < 18; i++)
	{
		int d = 1;

		/* Pick a location */
		scatter(&m_ptr->wpos, &y, &x, m_ptr->fy, m_ptr->fx, d, 0);
		/* Require an "empty" floor grid */
		if (!cave_empty_bold(zcave, y, x)) continue;
#if 0
		/* Create a new monster (awake, no groups) */
		result = place_monster_aux(&m_ptr->wpos, y, x, m_ptr->r_idx, FALSE, FALSE, m_ptr->clone + 10, m_ptr->clone_summoning + 1);
#else
		result = place_monster_one(&m_ptr->wpos, y, x, m_ptr->r_idx,
				(m_ptr->ego && magik(CLONE_EGO_CHANCE)) ? m_ptr->ego :
				pick_ego_monster(m_ptr->r_idx, getlevel(&m_ptr->wpos)),
				0, FALSE, m_ptr->clone + 10, m_ptr->clone_summoning + 1);
#endif	// 0

		/* Done */
		break;
	}

	/* Result */
	return (result);
}





/*
 * Dump a message describing a monster's reaction to damage
 *
 * Technically should attempt to treat "Beholder"'s as jelly's
 */
void message_pain(int Ind, int m_idx, int dam)
{
	long                    oldhp, newhp, tmp;
	int                             percentage;

	monster_type		*m_ptr = &m_list[m_idx];
        monster_race            *r_ptr = race_inf(m_ptr);

	char                    m_name[80];


	/* Get the monster name */
	monster_desc(Ind, m_name, m_idx, 0);


	/* some monsters don't react at all (Target Dummy) */
	if (r_ptr->flags7 & RF7_NEVER_ACT) {
		if (r_ptr->flags1 & RF1_UNIQUE)
			msg_format(Ind, "%^s reels from \377e%d \377wdamage.", m_name, dam);
		else
			msg_format(Ind, "%^s reels from \377g%d \377wdamage.", m_name, dam);
		return;
	}


	/* Notice non-damage */
	if (dam == 0)
	{
		msg_format(Ind, "%^s is unharmed.", m_name);
		return;
	}

	/* Note -- subtle fix -CFT */
	newhp = (long)(m_ptr->hp);
	oldhp = newhp + (long)(dam);
	tmp = (newhp * 100L) / oldhp;
	percentage = (int)(tmp);

	/* DEG Modified to give damage information */
	/* Jelly's, Mold's, Vortex's, Quthl's */
	
	if (strchr("jmvQ", r_ptr->d_char) && (r_ptr->flags1 & RF1_UNIQUE))
	{
		if (percentage > 95)
			msg_format(Ind, "%^s barely notices the \377e%d \377wdamage.", m_name, dam);
		else if (percentage > 75)
			msg_format(Ind, "%^s flinches from \377e%d \377wdamage.", m_name, dam);
		else if (percentage > 50)
			msg_format(Ind, "%^s squelches from \377e%d \377wdamage.", m_name, dam);
		else if (percentage > 35)
			msg_format(Ind, "%^s quivers in pain from \377e%d \377wdamage.", m_name, dam);
		else if (percentage > 20)
			msg_format(Ind, "%^s writhes about from \377e%d \377wdamage.", m_name, dam);
		else if (percentage > 10)
			msg_format(Ind, "%^s writhes in agony from \377e%d \377wdamage.", m_name, dam);
		else
			msg_format(Ind, "%^s jerks limply from \377e%d \377wdamage.", m_name, dam);
	}
	else if (strchr("jmvQ", r_ptr->d_char))
	{
		if (percentage > 95)
			msg_format(Ind, "%^s barely notices the \377g%d \377wdamage.", m_name, dam);
		else if (percentage > 75)
			msg_format(Ind, "%^s flinches from \377g%d \377wdamage.", m_name, dam);
		else if (percentage > 50)
			msg_format(Ind, "%^s squelches from \377g%d \377wdamage.", m_name, dam);
		else if (percentage > 35)
			msg_format(Ind, "%^s quivers in pain from \377g%d \377wdamage.", m_name, dam);
		else if (percentage > 20)
			msg_format(Ind, "%^s writhes about from \377g%d \377wdamage.", m_name, dam);
		else if (percentage > 10)
			msg_format(Ind, "%^s writhes in agony from \377g%d \377wdamage.", m_name, dam);
		else
			msg_format(Ind, "%^s jerks limply from \377g%d \377wdamage.", m_name, dam);
	}

	/* Dogs and Hounds */
	else if (strchr("CZ", r_ptr->d_char) && (r_ptr->flags1 & RF1_UNIQUE))
	{
		if (percentage > 95)
			msg_format(Ind, "%^s shrugs off the attack of \377e%d \377wdamage.", m_name, dam);
		else if (percentage > 75)
			msg_format(Ind, "%^s snarls with pain from \377e%d \377wdamage.", m_name, dam);
		else if (percentage > 50)
			msg_format(Ind, "%^s yelps in pain from \377e%d \377wdamage.", m_name, dam);
		else if (percentage > 35)
			msg_format(Ind, "%^s howls in pain from \377e%d \377wdamage.", m_name, dam);
		else if (percentage > 20)
			msg_format(Ind, "%^s howls in agony from \377e%d \377wdamage.", m_name, dam);
		else if (percentage > 10)
			msg_format(Ind, "%^s writhes in agony from \377e%d \377wdamage.", m_name,dam);
		else
			msg_format(Ind, "%^s yelps feebly from \377e%d \377wdamage.", m_name, dam);
	}
	else if (strchr("CZ", r_ptr->d_char))
	{
		if (percentage > 95)
			msg_format(Ind, "%^s shrugs off the attack of \377g%d \377wdamage.", m_name, dam);
		else if (percentage > 75)
			msg_format(Ind, "%^s snarls with pain from \377g%d \377wdamage.", m_name, dam);
		else if (percentage > 50)
			msg_format(Ind, "%^s yelps in pain from \377g%d \377wdamage.", m_name, dam);
		else if (percentage > 35)
			msg_format(Ind, "%^s howls in pain from \377g%d \377wdamage.", m_name, dam);
		else if (percentage > 20)
			msg_format(Ind, "%^s howls in agony from \377g%d \377wdamage.", m_name, dam);
		else if (percentage > 10)
			msg_format(Ind, "%^s writhes in agony from \377g%d \377wdamage.", m_name,dam);
		else
			msg_format(Ind, "%^s yelps feebly from \377g%d \377wdamage.", m_name, dam);
	}

	/* One type of monsters (ignore,squeal,shriek) */
	else if (strchr("FIKMRSXabclqrst", r_ptr->d_char) && (r_ptr->flags1 & RF1_UNIQUE))
	{
		if (percentage > 95)
			msg_format(Ind, "%^s ignores the attack of \377e%d \377wdamage.", m_name, dam);
		else if (percentage > 75)
			msg_format(Ind, "%^s grunts with pain from \377e%d \377wdamage.", m_name ,dam);
		else if (percentage > 50)
			msg_format(Ind, "%^s squeals in pain from \377e%d \377wdamage.", m_name, dam);
		else if (percentage > 35)
			msg_format(Ind, "%^s shrieks in pain from \377e%d \377wdamage.", m_name, dam);
		else if (percentage > 20)
			msg_format(Ind, "%^s shrieks in agony from \377e%d \377wdamage.", m_name, dam);
		else if (percentage > 10)
			msg_format(Ind, "%^s writhes in agony from \377e%d \377wdamage.", m_name, dam);
		else
			msg_format(Ind, "%^s cries out feebly from \377e%d \377wdamage.", m_name, dam);
	}

	/* Another type of monsters (shrug,cry,scream) */
	else if (strchr("FIKMRSXabclqrst", r_ptr->d_char))
	{
		if (percentage > 95)
			msg_format(Ind, "%^s ignores the attack of \377g%d \377wdamage.", m_name, dam);
		else if (percentage > 75)
			msg_format(Ind, "%^s grunts with pain from \377g%d \377wdamage.", m_name ,dam);
		else if (percentage > 50)
			msg_format(Ind, "%^s squeals in pain from \377g%d \377wdamage.", m_name, dam);
		else if (percentage > 35)
			msg_format(Ind, "%^s shrieks in pain from \377g%d \377wdamage.", m_name, dam);
		else if (percentage > 20)
			msg_format(Ind, "%^s shrieks in agony from \377g%d \377wdamage.", m_name, dam);
		else if (percentage > 10)
			msg_format(Ind, "%^s writhes in agony from \377g%d \377wdamage.", m_name, dam);
		else
			msg_format(Ind, "%^s cries out feebly from \377g%d \377wdamage.", m_name, dam);
	}

	/* Another type of monsters (shrug,cry,scream) */
	else if (r_ptr->flags1 & RF1_UNIQUE)
	{
		if (percentage > 95)
			msg_format(Ind, "%^s shrugs off the attack of \377e%d \377wdamage.", m_name, dam);
		else if (percentage > 75)
			msg_format(Ind, "%^s grunts with pain from \377e%d \377wdamage.", m_name, dam);
		else if (percentage > 50)
			msg_format(Ind, "%^s cries out in pain from \377e%d \377wdamage.", m_name, dam);
		else if (percentage > 35)
			msg_format(Ind, "%^s screams in pain from \377e%d \377wdamage.", m_name, dam);
		else if (percentage > 20)
			msg_format(Ind, "%^s screams in agony from \377e%d \377wdamage.", m_name, dam);
		else if (percentage > 10)
			msg_format(Ind, "%^s writhes in agony from \377e%d \377wdamage.", m_name, dam);
		else
			msg_format(Ind, "%^s cries out feebly from \377e%d \377wdamage.", m_name, dam);
	}
	else
	{
		if (percentage > 95)
			msg_format(Ind, "%^s shrugs off the attack of \377g%d \377wdamage.", m_name, dam);
		else if (percentage > 75)
			msg_format(Ind, "%^s grunts with pain from \377g%d \377wdamage.", m_name, dam);
		else if (percentage > 50)
			msg_format(Ind, "%^s cries out in pain from \377g%d \377wdamage.", m_name, dam);
		else if (percentage > 35)
			msg_format(Ind, "%^s screams in pain from \377g%d \377wdamage.", m_name, dam);
		else if (percentage > 20)
			msg_format(Ind, "%^s screams in agony from \377g%d \377wdamage.", m_name, dam);
		else if (percentage > 10)
			msg_format(Ind, "%^s writhes in agony from \377g%d \377wdamage.", m_name, dam);
		else
			msg_format(Ind, "%^s cries out feebly from \377g%d \377wdamage.", m_name, dam);
	}
}



/*
 * Learn about an "observed" resistance.
 */
void update_smart_learn(int m_idx, int what)
{

#ifdef DRS_SMART_OPTIONS

	monster_type *m_ptr = &m_list[m_idx];

        monster_race *r_ptr = race_inf(m_ptr);


	/* Not allowed to learn */
	if (!smart_learn) return;

	/* Too stupid to learn anything */
	if (r_ptr->flags2 & RF2_STUPID) return;

	/* Not intelligent, only learn sometimes */
	if (!(r_ptr->flags2 & RF2_SMART) && (rand_int(100) < 50)) return;


	/* XXX XXX XXX */

	/* Analyze the knowledge */
	switch (what)
	{
		case DRS_ACID:
		if (p_ptr->resist_acid) m_ptr->smart |= SM_RES_ACID;
		if (p_ptr->oppose_acid) m_ptr->smart |= SM_OPP_ACID;
		if (p_ptr->immune_acid) m_ptr->smart |= SM_IMM_ACID;
		break;

		case DRS_ELEC:
		if (p_ptr->resist_elec) m_ptr->smart |= SM_RES_ELEC;
		if (p_ptr->oppose_elec) m_ptr->smart |= SM_OPP_ELEC;
		if (p_ptr->immune_elec) m_ptr->smart |= SM_IMM_ELEC;
		break;

		case DRS_FIRE:
		if (p_ptr->resist_fire) m_ptr->smart |= SM_RES_FIRE;
		if (p_ptr->oppose_fire) m_ptr->smart |= SM_OPP_FIRE;
		if (p_ptr->immune_fire) m_ptr->smart |= SM_IMM_FIRE;
		break;

		case DRS_COLD:
		if (p_ptr->resist_cold) m_ptr->smart |= SM_RES_COLD;
		if (p_ptr->oppose_cold) m_ptr->smart |= SM_OPP_COLD;
		if (p_ptr->immune_cold) m_ptr->smart |= SM_IMM_COLD;
		break;

		case DRS_POIS:
		if (p_ptr->resist_pois) m_ptr->smart |= SM_RES_POIS;
		if (p_ptr->oppose_pois) m_ptr->smart |= SM_OPP_POIS;
		break;


		case DRS_NETH:
		if (p_ptr->resist_neth) m_ptr->smart |= SM_RES_NETH;
		if (p_ptr->immune_neth) m_ptr->smart |= SM_RES_NETH;
		break;

		case DRS_LITE:
		if (p_ptr->resist_lite) m_ptr->smart |= SM_RES_LITE;
		break;

		case DRS_DARK:
		if (p_ptr->resist_dark) m_ptr->smart |= SM_RES_DARK;
		break;

		case DRS_FEAR:
		if (p_ptr->resist_fear) m_ptr->smart |= SM_RES_FEAR;
		break;

		case DRS_CONF:
		if (p_ptr->resist_conf) m_ptr->smart |= SM_RES_CONF;
		break;

		case DRS_CHAOS:
		if (p_ptr->resist_chaos) m_ptr->smart |= SM_RES_CHAOS;
		break;

		case DRS_DISEN:
		if (p_ptr->resist_disen) m_ptr->smart |= SM_RES_DISEN;
		break;

		case DRS_BLIND:
		if (p_ptr->resist_blind) m_ptr->smart |= SM_RES_BLIND;
		break;

		case DRS_NEXUS:
		if (p_ptr->resist_nexus) m_ptr->smart |= SM_RES_NEXUS;
		break;

		case DRS_SOUND:
		if (p_ptr->resist_sound) m_ptr->smart |= SM_RES_SOUND;
		break;

		case DRS_SHARD:
		if (p_ptr->resist_shard) m_ptr->smart |= SM_RES_SHARD;
		break;


		case DRS_FREE:
		if (p_ptr->free_act) m_ptr->smart |= SM_IMM_FREE;
		break;

		case DRS_MANA:
		if (!p_ptr->msp) m_ptr->smart |= SM_IMM_MANA;
		break;
	}

#endif /* DRS_SMART_OPTIONS */

}

/*
* Set the "m_idx" fields in the cave array to correspond
* to the objects in the "m_list".
*/
void setup_monsters(void)
{
	int i;
	monster_type *r_ptr;
	cave_type **zcave;

	for (i = 0; i < m_max; i++)
	{
		r_ptr = &m_list[i];
		/* setup the cave m_idx if the level has been 
		 * allocated.
		 */
		if((zcave=getcave(&r_ptr->wpos)))
			zcave[r_ptr->fy][r_ptr->fx].m_idx = i;
	}
}

/* Takes a monster name and returns an index, or 0 if no such monster
 * was found.
 */
int race_index(char * name)
{
	int i;

	/* for each monster race */
	for (i = 1; i <= alloc_race_size; i++)
	{
		if (!strcmp(&r_name[r_info[i].name],name)) return i;
	}
	return 0;
}

monster_race* r_info_get(monster_type *m_ptr)
{
	/* golem? */
	if (m_ptr->special) return (m_ptr->r_ptr);
#ifdef RANDUNIS
	else if (m_ptr->ego) return (race_info_idx((m_ptr)->r_idx, (m_ptr)->ego, (m_ptr)->name3));
	else return (&r_info[m_ptr->r_idx]);
#else
	else return (&r_info[m_ptr->r_idx]);
#endif	// RANDUNIS
}

cptr r_name_get(monster_type *m_ptr)
{
	static char buf[100];

	if (m_ptr->special)
	{
		cptr p = (m_ptr->owner)?lookup_player_name(m_ptr->owner):"**INTERNAL BUG**";
		if (p == NULL) p = "**INTERNAL BUG**";
		switch (m_ptr->r_idx - 1)
		{
			case SV_GOLEM_WOOD:
				snprintf(buf, sizeof(buf), "%s's Wood Golem", p);
				break;
			case SV_GOLEM_COPPER:
				snprintf(buf, sizeof(buf), "%s's Copper Golem", p);
				break;
			case SV_GOLEM_IRON:
				snprintf(buf, sizeof(buf), "%s's Iron Golem", p);
				break;
			case SV_GOLEM_ALUM:
				snprintf(buf, sizeof(buf), "%s's Aluminium Golem", p);
				break;
			case SV_GOLEM_SILVER:
				snprintf(buf, sizeof(buf), "%s's Silver Golem", p);
				break;
			case SV_GOLEM_GOLD:
				snprintf(buf, sizeof(buf), "%s's Gold Golem", p);
				break;
			case SV_GOLEM_MITHRIL:
				snprintf(buf, sizeof(buf), "%s's Mithril Golem", p);
				break;
			case SV_GOLEM_ADAM:
				snprintf(buf, sizeof(buf), "%s's Adamantite Golem", p);
				break;
		}
		return (buf);
	}
#ifdef RANDUNIS
		else if(m_ptr->ego)
		{
			if (re_info[m_ptr->ego].before)
			{
				snprintf(buf, sizeof(buf), "%s %s", re_name + re_info[m_ptr->ego].name,
						r_name + r_info[m_ptr->r_idx].name);
			}
			else
			{
				snprintf(buf, sizeof(buf), "%s %s", r_name + r_info[m_ptr->r_idx].name,
						re_name + re_info[m_ptr->ego].name);
			}
			return (buf);
		}
#endif	// RANDUNIS
        else return (r_name + r_info[m_ptr->r_idx].name);
}

#ifdef RANDUNIS
/* Will add, sub, .. */
static s32b modify_aux(s32b a, s32b b, char mod)
{
        switch (mod)
        {
                case MEGO_ADD:
                        return (a + b);
                        break;
                case MEGO_SUB:
                        return (a - b);
                        break;
                case MEGO_FIX:
                        return (b);
                        break;
                case MEGO_PRC:
                        return (a * b / 100);
                        break;
                default:
                        s_printf("WARNING, unmatching MEGO(%d).", mod);
                        return (0);
        }
}

#define MODIFY_AUX(o, n) ((o) = modify_aux((o), (n) >> 2, (n) & 3))
#define MODIFY(o, n, min) MODIFY_AUX(o, n); (o) = ((o) < (min))?(min):(o)

/* Is this ego ok for this monster ? */
bool mego_ok(int r_idx, int ego)
{
        monster_ego *re_ptr = &re_info[ego];
        monster_race *r_ptr = &r_info[r_idx];
        bool ok = FALSE;
        int i;

		/* missing number */
		if (!re_ptr->name) return FALSE;

        /* needed flags */
        if (re_ptr->flags1 && ((re_ptr->flags1 & r_ptr->flags1) != re_ptr->flags1)) return FALSE;
        if (re_ptr->flags2 && ((re_ptr->flags2 & r_ptr->flags2) != re_ptr->flags2)) return FALSE;
        if (re_ptr->flags3 && ((re_ptr->flags3 & r_ptr->flags3) != re_ptr->flags3)) return FALSE;
#if 1
        if (re_ptr->flags7 && ((re_ptr->flags7 & r_ptr->flags7) != re_ptr->flags7)) return FALSE;
        if (re_ptr->flags8 && ((re_ptr->flags8 & r_ptr->flags8) != re_ptr->flags8)) return FALSE;
        if (re_ptr->flags9 && ((re_ptr->flags9 & r_ptr->flags9) != re_ptr->flags9)) return FALSE;
#endif

        /* unwanted flags */
        if (re_ptr->hflags1 && (re_ptr->hflags1 & r_ptr->flags1)) return FALSE;
        if (re_ptr->hflags2 && (re_ptr->hflags2 & r_ptr->flags2)) return FALSE;
        if (re_ptr->hflags3 && (re_ptr->hflags3 & r_ptr->flags3)) return FALSE;
#if 1
        if (re_ptr->hflags7 && (re_ptr->hflags7 & r_ptr->flags7)) return FALSE;
        if (re_ptr->hflags8 && (re_ptr->hflags8 & r_ptr->flags8)) return FALSE;
        if (re_ptr->hflags9 && (re_ptr->hflags9 & r_ptr->flags9)) return FALSE;
#endif

        /* Need good race -- IF races are specified */
        if (re_ptr->r_char[0])
        {
                for (i = 0; i < 10; i++)
                {
                        if (r_ptr->d_char == re_ptr->r_char[i]) ok = TRUE;
                }
                if (!ok) return FALSE;
        }
        if (re_ptr->nr_char[0])
        {
                for (i = 0; i < 10; i++)
                {
                        if (r_ptr->d_char == re_ptr->nr_char[i]) return (FALSE);
                }
        }

        /* Passed all tests ? */
        return TRUE;
}

/* Choose an ego type */
/* 'Level' is a transitional index for dun_depth.	- Jir -
 */
int pick_ego_monster(int r_idx, int Level)
{
        /* Assume no ego */
        int ego = 0, lvl;
        int tries = MAX_RE_IDX + 10;
        monster_ego *re_ptr;

        /* No townspeople ego */
        if (!r_info[r_idx].level) return 0;
	
#ifdef HALLOWEEN
	/* No Great Pumpkin ego */
	if (r_idx == 1086 || r_idx == 1087 || r_idx == 1088) return 0;
#endif

        /* First are we allowed to find an ego */
        if (!magik(MEGO_CHANCE)) return 0;

        /* Ego Uniques are NOT to be created */
        if ((r_info[r_idx].flags1 & RF1_UNIQUE)) return 0;

        /* Lets look for one */
        while(tries--)
        {
                /* Pick one */
                ego = rand_range(1, MAX_RE_IDX - 1);
                re_ptr = &re_info[ego];

                /*  No hope so far */
                if (!mego_ok(r_idx, ego)) continue;

                /* Not too much OoD */
                lvl = r_info[r_idx].level;
                MODIFY(lvl, re_ptr->level, 0);
                lvl -= ((Level / 2) + (rand_int(Level / 2)));
                if (lvl < 1) lvl = 1;
                if (rand_int(lvl)) continue;
                // if (rand_int(lvl * 2 - 1)) continue;	/* less OoD */

				/* Hack -- Depth monsters may NOT be created out of depth */
				if ((re_ptr->mflags1 & RF1_FORCE_DEPTH) && (lvl > 1))
				{
					/* Cannot create */
					continue;
				}

                /* Each ego types have a rarity */
                if (rand_int(re_ptr->rarity)) continue;

				/* (Remove me) */
#if DEBUG_LEVEL > 2
				s_printf("ego %d(%s)(%s) is generated.\n", ego, re_name + re_ptr->name, r_name + r_info[r_idx].name);
#endif

                /* We finanly got one ? GREAT */
                return ego;
        }

        /* Found none ? so sad, well no ego for the time being */
        return 0;
}

/* Pick a randuni (not implemented yet) */
int pick_randuni(int r_idx, int Level)
{
	return 0;
#if 0
        /* Assume no ego */
        int ego = 0, lvl;
        int tries = max_re_idx + 10;
        monster_ego *re_ptr;

        /* No townspeople ego */
        if (!r_info[r_idx].level) return 0;

        /* First are we allowed to find an ego */
        if (!magik(MEGO_CHANCE)) return 0;


        /* Lets look for one */
        while(tries--)
        {
                /* Pick one */
                ego = rand_range(1, max_re_idx - 1);
                re_ptr = &re_info[ego];

                /*  No hope so far */
                if (!mego_ok(r_idx, ego)) continue;

                /* Not too much OoD */
                lvl = r_info[r_idx].level;
                MODIFY(lvl, re_ptr->level, 0);
                lvl -= ((dlev / 2) + (rand_int(dlev / 2)));
                if (lvl < 1) lvl = 1;
                if (rand_int(lvl)) continue;

                /* Each ego types have a rarity */
                if (rand_int(re_ptr->rarity)) continue;

                /* We finanly got one ? GREAT */
                return ego;
        }

        /* Found none ? so sad, well no ego for the time being */
        return 0;
#endif
}

/*
 * Return a (monster_race*) with the combinaison of the monster
 * proprieties, the ego type and randuni id.
 * (randuni parts are not done yet..	- Jir -)
 */
static monster_race* race_info_idx(int r_idx, int ego, int randuni)
{
        static monster_race race;
        monster_ego *re_ptr = &re_info[ego];
        monster_race *r_ptr = &r_info[r_idx], *nr_ptr = &race;
        int i;

        /* No work needed */
        if (!ego) return r_ptr;

        /* Copy the base monster */
        COPY(nr_ptr, r_ptr, monster_race);

        /* Adjust the values */
        for (i = 0; i < 4; i++)
        {
                s32b j, k;

                j = modify_aux(nr_ptr->blow[i].d_dice, re_ptr->blow[i].d_dice, re_ptr->blowm[i][0]);
                if (j < 0) j = 0;
                if (j > 255) j = 255;
                k = modify_aux(nr_ptr->blow[i].d_side, re_ptr->blow[i].d_side, re_ptr->blowm[i][1]);
                if (k < 0) k = 0;
                if (k > 255) k = 255;

                nr_ptr->blow[i].d_dice = j;
                nr_ptr->blow[i].d_side = k;

                if (re_ptr->blow[i].method) nr_ptr->blow[i].method = re_ptr->blow[i].method;
                if (re_ptr->blow[i].effect) nr_ptr->blow[i].effect = re_ptr->blow[i].effect;
        }

        MODIFY(nr_ptr->hdice, re_ptr->hdice, 1);
        MODIFY(nr_ptr->hside, re_ptr->hside, 1);

        MODIFY(nr_ptr->ac, re_ptr->ac, 0);

        MODIFY(nr_ptr->sleep, re_ptr->sleep, 0);

        MODIFY(nr_ptr->aaf, re_ptr->aaf, 1);

		/* Hack - avoid to be back to 0 (consider using s16b) */
		i = nr_ptr->speed;
        MODIFY(i, re_ptr->speed, 50);
        MODIFY(nr_ptr->speed, re_ptr->speed, 50);

		if (nr_ptr->speed < i) nr_ptr->speed = 255;

        MODIFY(nr_ptr->mexp, re_ptr->mexp, 0);

//        MODIFY(nr_ptr->weight, re_ptr->weight, 10);

        nr_ptr->freq_inate = re_ptr->freq_inate;
        nr_ptr->freq_spell = re_ptr->freq_spell;

        MODIFY(nr_ptr->level, re_ptr->level, 1);

        /* Take off some flags */
        nr_ptr->flags1 &= ~(re_ptr->nflags1);
        nr_ptr->flags2 &= ~(re_ptr->nflags2);
        nr_ptr->flags3 &= ~(re_ptr->nflags3);
        nr_ptr->flags4 &= ~(re_ptr->nflags4);
        nr_ptr->flags5 &= ~(re_ptr->nflags5);
        nr_ptr->flags6 &= ~(re_ptr->nflags6);
        nr_ptr->flags7 &= ~(re_ptr->nflags7);
        nr_ptr->flags8 &= ~(re_ptr->nflags8);
        nr_ptr->flags9 &= ~(re_ptr->nflags9);

        /* Add some flags */
        nr_ptr->flags1 |= re_ptr->mflags1;
        nr_ptr->flags2 |= re_ptr->mflags2;
        nr_ptr->flags3 |= re_ptr->mflags3;
        nr_ptr->flags4 |= re_ptr->mflags4;
        nr_ptr->flags5 |= re_ptr->mflags5;
        nr_ptr->flags6 |= re_ptr->mflags6;
        nr_ptr->flags7 |= re_ptr->mflags7;
        nr_ptr->flags8 |= re_ptr->mflags8;
        nr_ptr->flags9 |= re_ptr->mflags9;

        /* Change the char/attr is needed */
        if (re_ptr->d_char != MEGO_CHAR_ANY)
        {
                nr_ptr->d_char = re_ptr->d_char;
                nr_ptr->x_char = re_ptr->d_char;
        }
        if (re_ptr->d_attr != MEGO_CHAR_ANY)
        {
                nr_ptr->d_attr = re_ptr->d_attr;
                nr_ptr->x_attr = re_ptr->d_attr;
        }

        /* And finanly return a pointer to a fully working monster race */
        return nr_ptr;
}

#endif	// RANDUNIS

#ifdef MONSTER_INVENTORY
/*
 * Drop all items carried by a monster
 */
void monster_drop_carried_objects(monster_type *m_ptr)
{
	s16b this_o_idx, next_o_idx = 0;
	object_type forge;
	object_type *o_ptr;
	object_type *q_ptr;


	/* Drop objects being carried */
	for (this_o_idx = m_ptr->hold_o_idx; this_o_idx; this_o_idx = next_o_idx)
	{
		/* Acquire object */
		o_ptr = &o_list[this_o_idx];

		/* Acquire next object */
		next_o_idx = o_ptr->next_o_idx;

		/* Paranoia */
		o_ptr->held_m_idx = 0;

		/* Get local object */
		q_ptr = &forge;

		/* Copy the object */
		object_copy(q_ptr, o_ptr);
		q_ptr->next_o_idx = 0;

		/* Delete the object */
		delete_object_idx(this_o_idx, FALSE);

		/* the_sandman - Perhaps we can filter out the nothings here?
		 */
//		char* tempbuf = (char*) malloc(sizeof(char)*80);
//		object_desc(Ind, tempbuf, o_ptr, 0, 0);
//		if (!strcmp(tempbuf, "(nothing)")) {
			/* Drop it */
			drop_near(q_ptr, -1, &m_ptr->wpos, m_ptr->fy, m_ptr->fx);
//		}
//		free(tempbuf);
	}

	/* Forget objects */
	m_ptr->hold_o_idx = 0;
}

void monster_carry(monster_type *m_ptr, int m_idx, object_type *q_ptr)
{
	object_type *o_ptr;

	/* Get new object */
	int o_idx = o_pop();

	if (o_idx)
	{
		/* Get the item */
		o_ptr = &o_list[o_idx];

		/* Structure copy */
		object_copy(o_ptr, q_ptr);

		/* Build a stack */
		o_ptr->next_o_idx = m_ptr->hold_o_idx;

		o_ptr->held_m_idx = m_idx;
		o_ptr->ix = 0;
		o_ptr->iy = 0;

		m_ptr->hold_o_idx = o_idx;
	}

	else
	{
		/* Hack -- Preserve artifacts */
		if (q_ptr->name1)
		{
			handle_art_d(q_ptr->name1);
		}
	}
}

#endif	// MONSTER_INVENTORY


/* Already in wild.c */
#define monster_dungeon(r_idx) dungeon_aux(r_idx)


static bool monster_deep_water(int r_idx)
{
	monster_race *r_ptr = &r_info[r_idx];

	if (!monster_dungeon(r_idx)) return FALSE;

	if (r_ptr->flags7 & RF7_AQUATIC)
		return TRUE;
	else
		return FALSE;
}


static bool monster_shallow_water(int r_idx)
{
	monster_race *r_ptr = &r_info[r_idx];

	if (!monster_dungeon(r_idx)) return FALSE;

	if (r_ptr->flags2 & RF2_AURA_FIRE)
		return FALSE;
	else
		return TRUE;
}


static bool monster_lava(int r_idx)
{
	monster_race *r_ptr = &r_info[r_idx];

	if (!monster_dungeon(r_idx)) return FALSE;

	if (((r_ptr->flags3 & RF3_IM_FIRE) ||
	     (r_ptr->flags9 & RF9_RES_FIRE) ||
	     (r_ptr->flags3 & RF3_RES_PLAS) ||
	     (r_ptr->flags7 & RF7_CAN_FLY)) &&
	    !(r_ptr->flags3 & RF3_AURA_COLD))
		return TRUE;
	else
		return FALSE;
}


/*
 * Check if monster can cross terrain
 */
bool monster_can_cross_terrain(byte feat, monster_race *r_ptr)
{
	/* Deep water */
	if (feat == FEAT_DEEP_WATER)
	{
		if ((r_ptr->flags7 & RF7_AQUATIC) ||
		    (r_ptr->flags7 & RF7_CAN_FLY) ||
		    (r_ptr->flags9 & RF9_IM_WATER) ||
		    (r_ptr->flags7 & RF7_CAN_SWIM))
			return TRUE;
		else
			return FALSE;
	}
	/* Shallow water */
	else if (feat == FEAT_SHAL_WATER)
	{
		if (r_ptr->flags2 & RF2_AURA_FIRE)
			return FALSE;
		else
			return TRUE;
	}
	/* Aquatic monster */
	else if ((r_ptr->flags7 & RF7_AQUATIC) &&
		    !(r_ptr->flags7 & RF7_CAN_FLY))
	{
		return FALSE;
	}
	/* Lava */
	else if ((feat == FEAT_SHAL_LAVA) ||
	    (feat == FEAT_DEEP_LAVA))
	{
		if ((r_ptr->flags3 & RF3_IM_FIRE) ||
		    (r_ptr->flags9 & RF9_RES_FIRE) ||
		    (r_ptr->flags3 & RF3_RES_PLAS) ||
		    (r_ptr->flags7 & RF7_CAN_FLY))
			return TRUE;
		else
			return FALSE;
	}

	return TRUE;
}


void set_mon_num2_hook(worldpos *wpos, int y, int x)
{
	cave_type **zcave;
	if (!in_bounds(y, x)) return;
	if(!(zcave=getcave(wpos))) return;

	/* Set the monster list */
	switch (zcave[y][x].feat)
	{
	case FEAT_SHAL_WATER:
		get_mon_num2_hook = monster_shallow_water;
		break;
	case FEAT_DEEP_WATER:
		get_mon_num2_hook = monster_deep_water;
		break;
	case FEAT_DEEP_LAVA:
	case FEAT_SHAL_LAVA:
		get_mon_num2_hook = monster_lava;
		break;
	default:
		get_mon_num2_hook = NULL;
		break;
	}
}

-- handle the holy curing school

function get_healing_percents2(limit_lev)
	local perc
	perc = get_level(Ind, HHEALING_I, 31)
	if limit_lev ~= 0 then
		if perc > (limit_lev * 3) / 5 then
			perc = (limit_lev * 3) / 5 + (perc - (limit_lev * 3) / 5) / 3
		end
	end
	return 25 + perc
end
function get_healing_cap2(limit_lev)
	local pow
	pow = get_level(Ind, HHEALING_I, 417)
	if limit_lev ~= 0 then
		if pow > limit_lev * 8 then
			pow = limit_lev * 8 + (pow - limit_lev * 8) / 3
		end
	end
	if pow > 400 then
		pow = 400
	end
	return pow
end
function get_healing_power2(limit_lev)
	local pow, cap
	pow = player.mhp * get_healing_percents2(limit_lev) / 100
	cap = get_healing_cap2(limit_lev)
	if pow > cap then
		pow = cap
	end
	return pow
end

function get_curewounds_dice(limit_lev)
	local hd
	local lev

	hd = get_level(Ind, HCUREWOUNDS_I, 27) + 1

	if limit_lev ~= 0 then
		if hd > limit_lev / 2 + 2 then
			hd = limit_lev / 2 + 2 + (hd - (limit_lev / 2 + 2)) / 3
		end
	end

	if (hd > 14) then hd = 14 end
	return hd
end
function get_curewounds_power(limit_lev)
	local pow
--[[
	pow = get_level(Ind, HCUREWOUNDS, 200)
	if pow > 112 then
		pow = 112
	end
]]
	pow = damroll(get_curewounds_dice(limit_lev), 8)
	return pow
end

-- Keep consistent with GHOST_XP_LOST -- hardcoded mess
function get_exp_loss()
	local pow
	--ENABLE_INSTANT_RES?
	if (def_hack("TEMP0", nil) == 1) then
		pow = (36 * (735 - (5 * get_level(Ind, HRESURRECT)))) / 735
		if pow < 30 then
			pow = 30
		end
	else
		pow = (41 * (120 - get_level(Ind, HRESURRECT))) / 120
		if pow < 33 then
			pow = 33
		end
	end
	return pow
end

HCUREWOUNDS_I = add_spell {
	["name"] = 	"Cure Wounds I",
	["school"] = 	{SCHOOL_HCURING},
	["am"] = 	75,
	["level"] = 	3,
	["mana"] = 	5,
	["mana_max"] = 	5,
	["fail"] = 	15,
	["stat"] = 	A_WIS,
	["direction"] = TRUE,
	["spell"] = 	function(args)
			local status_ailments
			status_ailments = 0
			--hacks to cure effects same as potions would
			if get_level(Ind, HCUREWOUNDS_I, 50) >= 9 then
				status_ailments = status_ailments + 2048
			end
			fire_grid_bolt(Ind, GF_HEAL_PLAYER, args.dir, status_ailments + get_curewounds_power(13), " points at your wounds.")
	end,
	["info"] = 	function()
--			return "heal "..get_curewounds_power(13)
			return "heal "..get_curewounds_dice(13).."d8"
	end,
	["desc"] = 	{
		"Heals a certain amount of hitpoints of a friendly target.",
		"Caps at 14d8 (same as potion of cure critical wounds).",
		"Also cures blindness and cuts at level 9.",
	}
}
HCUREWOUNDS_II = add_spell {
	["name"] = 	"Cure Wounds II",
	["school"] = 	{SCHOOL_HCURING},
	["am"] = 	75,
	["level"] = 	23,
	["mana"] = 	20,
	["mana_max"] = 	20,
	["fail"] = 	0,
	["stat"] = 	A_WIS,
	["direction"] = TRUE,
	["spell"] = 	function(args)
			local status_ailments
			status_ailments = 0
			--hacks to cure effects same as potions would
			if get_level(Ind, HCUREWOUNDS_II, 50) >= 9 then
				status_ailments = status_ailments + 8192 + 4096 + 2048
			else
				status_ailments = status_ailments + 4096 + 2048
			end
			fire_grid_bolt(Ind, GF_HEAL_PLAYER, args.dir, status_ailments + get_curewounds_power(0), " points at your wounds.")
	end,
	["info"] = 	function()
--			return "heal "..get_curewounds_power(0)
			return "heal "..get_curewounds_dice(0).."d8"
	end,
	["desc"] = 	{
		"Heals a certain amount of hitpoints of a friendly target.",
		"Caps at 14d8 (same as potion of cure critical wounds).",
		"Also cures blindness, cuts and confusion.",
		"Also cures stun at level 9.",
	}
}

HHEALING_I = add_spell {
	["name"] = 	"Heal I",
	["school"] = 	{SCHOOL_HCURING},
	["am"] = 	75,
	["level"] = 	3,
	["mana"] = 	5,
	["mana_max"] = 	5,
	["fail"] = 	25,
	["stat"] = 	A_WIS,
	["spell"] = 	function()
			local status_ailments
			status_ailments = 1024
			--hacks to cure effects same as potions would
			if get_level(Ind, HHEALING_I, 50) >= 10 then
				status_ailments = status_ailments + 4096 + 2048
			elseif get_level(Ind, HHEALING_I, 50) >= 4 then
				status_ailments = status_ailments + 2048
			end
			fire_ball(Ind, GF_HEAL_PLAYER, 0, status_ailments + get_healing_power2(15), 1, " points at your wounds.")
	end,
	["info"] = 	function()
			return "heal "..get_healing_percents2(15).."% (max "..get_healing_cap2(15)..") = "..get_healing_power2(15)
	end,
	["desc"] = 	{
		"Heals a percentage of your hitpoints up to a spell level-dependent cap.",
		"Also cures blindness and cuts at level 4 and confusion at level 10.",
		"Final cap is 400. Projecting heals nearby players for 3/4 of the amount.",
		"***Automatically projecting***",
	}
}
__lua_HHEALING = HHEALING_I
HHEALING_II = add_spell {
	["name"] = 	"Heal II",
	["school"] = 	{SCHOOL_HCURING},
	["am"] = 	75,
	["level"] = 	23,
	["mana"] = 	20,
	["mana_max"] = 	20,
	["fail"] = 	10,
	["stat"] = 	A_WIS,
	["spell"] = 	function()
			local status_ailments
			status_ailments = 1024 + 2048 + 4096
			--hacks to cure effects same as potions would
			if get_level(Ind, HHEALING_II, 50) >= 8 then
				status_ailments = status_ailments + 8192 + 4096 + 2048
			end
			fire_ball(Ind, GF_HEAL_PLAYER, 0, status_ailments + get_healing_power2(35), 1, " points at your wounds.")
	end,
	["info"] = 	function()
			return "heal "..get_healing_percents2(35).."% (max "..get_healing_cap2(35)..") = "..get_healing_power2(35)
	end,
	["desc"] = 	{
		"Heals a percentage of your hitpoints up to a spell level-dependent cap.",
		"Also cures blindness, cuts and confusion and at level 8 stun too.",
		"Final cap is 400. Projecting heals nearby players for 3/4 of the amount.",
		"***Automatically projecting***",
	}
}
HHEALING_III = add_spell {
	["name"] = 	"Heal III",
	["school"] = 	{SCHOOL_HCURING},
	["am"] = 	75,
	["level"] = 	40,
	["mana"] = 	70,
	["mana_max"] = 	70,
	["fail"] = 	-5,
	["stat"] = 	A_WIS,
	["spell"] = 	function()
			local status_ailments
			--hacks to cure effects same as potions would
			status_ailments = 1024 + 2048 + 4096 + 8192
			fire_ball(Ind, GF_HEAL_PLAYER, 0, status_ailments + get_healing_power2(0), 1, " points at your wounds.")
	end,
	["info"] = 	function()
			return "heal "..get_healing_percents2(0).."% (max "..get_healing_cap2(0)..") = "..get_healing_power2(0)
	end,
	["desc"] = 	{
		"Heals a percentage of your hitpoints up to a spell level-dependent cap.",
		"Also cures blindness, cuts, confusion and stun.",
		"Final cap is 400. Projecting heals nearby players for 3/4 of the amount.",
		"***Automatically projecting***",
	}
}

HDELCURSES_I = add_spell {
	["name"] = 	"Break Curses I",
	["school"] = 	SCHOOL_HCURING,
	["am"] =	75,
	["level"] = 	10,
	["mana"] = 	20,
	["mana_max"] = 	20,
	["fail"] = 	40,
	["stat"] = 	A_WIS,
	["spell"] = 	function()
			local done
			done = remove_curse(Ind)
			if done == TRUE then msg_print(Ind, "The curse is broken!") end
			end,
	["info"] = 	function()
			return ""
			end,
	["desc"] = 	{
			"Removes curses.",
	}
}
HDELCURSES_II = add_spell {
	["name"] = 	"Break Curses II",
	["school"] = 	SCHOOL_HCURING,
	["am"] =	75,
	["level"] = 	40,
	["mana"] = 	40,
	["mana_max"] = 	40,
	["fail"] = 	0,
	["stat"] = 	A_WIS,
	["spell"] = 	function()
			local done
			done = remove_all_curse(Ind)
			if done == TRUE then msg_print(Ind, "The curse is broken!") end
			end,
	["info"] = 	function()
			return ""
			end,
	["desc"] = 	{
			"Removes curses and heavy curses.",
	}
}

HHEALING2_I = add_spell {
	["name"] = 	"Cleansing Light I",
	["school"] = 	{SCHOOL_HCURING},
	["am"] = 	75,
	["level"] = 	18,
	["mana"] = 	12,
	["mana_max"] = 	12,
	["fail"] = 	30,
	["stat"] = 	A_WIS,
	["direction"] = FALSE,
	["spell"] = 	function()
			fire_cloud(Ind, GF_HEALINGCLOUD, 0, (1 + get_level(Ind, HHEALING2_I, 12)), (1 + get_level(Ind, HHEALING2_I, 10)), (5 + get_level(Ind, HHEALING2_I, 16)), 10, " calls the spirits")
			end,
	["info"] = 	function()
			return "heals " .. (get_level(Ind, HHEALING2_I, 12) + 1) .. " rad " .. (1 + get_level(Ind, HHEALING2_I, 10)) .. " dur " .. (5 + get_level(Ind, HHEALING2_I, 16))
			end,
	["desc"] = 	{ "Continuously heals you and those around you.", }
}
HHEALING2_II = add_spell {
	["name"] = 	"Cleansing Light II",
	["school"] = 	{SCHOOL_HCURING},
	["am"] = 	75,
	["level"] = 	29,
	["mana"] = 	35,
	["mana_max"] = 	35,
	["fail"] = 	15,
	["stat"] = 	A_WIS,
	["direction"] = FALSE,
	["spell"] = 	function()
			fire_cloud(Ind, GF_HEALINGCLOUD, 0, (1 + get_level(Ind, HHEALING2_I, 22)), (1 + get_level(Ind, HHEALING2_I, 10)), (5 + get_level(Ind, HHEALING2_I, 16)), 10, " calls the spirits")
			end,
	["info"] = 	function()
			return "heals " .. (get_level(Ind, HHEALING2_I, 22) + 1) .. " rad " .. (1 + get_level(Ind, HHEALING2_I, 10)) .. " dur " .. (5 + get_level(Ind, HHEALING2_I, 16))
			end,
	["desc"] = 	{ "Continuously heals you and those around you.", }
}
HHEALING2_III = add_spell {
	["name"] = 	"Cleansing Light III",
	["school"] = 	{SCHOOL_HCURING},
	["am"] = 	75,
	["level"] = 	40,
	["mana"] = 	100,
	["mana_max"] = 	100,
	["fail"] = 	0,
	["stat"] = 	A_WIS,
	["direction"] = FALSE,
	["spell"] = 	function()
			fire_cloud(Ind, GF_HEALINGCLOUD, 0, (1 + get_level(Ind, HHEALING2_I, 42)), (1 + get_level(Ind, HHEALING2_I, 10)), (5 + get_level(Ind, HHEALING2_I, 16)), 10, " calls the spirits")
			end,
	["info"] = 	function()
			return "heals " .. (get_level(Ind, HHEALING2_I, 42) + 1) .. " rad " .. (1 + get_level(Ind, HHEALING2_I, 10)) .. " dur " .. (5 + get_level(Ind, HHEALING2_I, 16))
			end,
	["desc"] = 	{ "Continuously heals you and those around you.", }
}

HCURING_I = add_spell {
	["name"] = 	"Curing I",
	["school"] = 	{SCHOOL_HCURING},
	["am"] = 	75,
	["level"] = 	3,
	["mana"] = 	4,
	["mana_max"] = 	4,
	["fail"] = 	20,
	["stat"] = 	A_WIS,
	["spell"] = 	function()
			if (player.food >= PY_FOOD_MAX) then
				set_food(Ind, PY_FOOD_MAX - 1)
			end
			if (player.poisoned ~= 0 and player.slow_poison == 0) then
				player.slow_poison = 1
			end
			fire_ball(Ind, GF_CURE_PLAYER, 0, 1 + 2, 1, " concentrates on your maladies.")
			end,
	["info"] = 	function()
			return ""
			end,
	["desc"] = 	{
			"Slows down the effect of poison and treats stomach ache.",
			"***Automatically projecting***",
	}
}
HCURING_II = add_spell {
	["name"] = 	"Curing II",
	["school"] = 	{SCHOOL_HCURING},
	["am"] = 	75,
	["level"] = 	12,
	["mana"] = 	10,
	["mana_max"] = 	10,
	["fail"] = 	20,
	["stat"] = 	A_WIS,
	["spell"] = 	function()
			if (player.food >= PY_FOOD_MAX) then
				set_food(Ind, PY_FOOD_MAX - 1)
			end
			set_poisoned(Ind, 0, 0)
			set_image(Ind, 0)
			fire_ball(Ind, GF_CURE_PLAYER, 0, 2 + 4 + 20, 1, " concentrates on your maladies.")
			end,
	["info"] = 	function()
			return ""
			end,
	["desc"] = 	{
			"Treats stomach ache,",
			"neutralizes poison and cures hallucinations.",
			"***Automatically projecting***",
	}
}
HCURING_III = add_spell {
	["name"] = 	"Curing III",
	["school"] = 	{SCHOOL_HCURING},
	["am"] = 	75,
	["level"] = 	21,
	["mana"] = 	25,
	["mana_max"] = 	25,
	["fail"] = 	20,
	["stat"] = 	A_WIS,
	["spell"] = 	function()
			if (player.food >= PY_FOOD_MAX) then
				set_food(Ind, PY_FOOD_MAX - 1)
			end
			set_confused(Ind, 0)
			set_blind(Ind, 0)
			set_stun(Ind, 0)
			set_poisoned(Ind, 0, 0)
			set_image(Ind, 0)
			fire_ball(Ind, GF_CURE_PLAYER, 0, 2 + 4 + 10 + 20, 1, " concentrates on your maladies.")
			end,
	["info"] = 	function()
			return ""
			end,
	["desc"] = 	{
			"Treats stomach ache,",
			"neutralizes poison and cures hallucinations,",
			"cures confusion, blindness and stun.",
			"***Automatically projecting***",
	}
}

HRESTORING = add_spell {
	["name"] = 	"Restoration",
	["school"] = 	{SCHOOL_HCURING},
	["am"] = 	75,
	["level"] = 	31,
	["mana"] = 	25,
	["mana_max"] = 	25,
	["fail"] = 	15,
	["stat"] = 	A_WIS,
	["spell"] = 	function()
			do_res_stat(Ind, A_STR)
			do_res_stat(Ind, A_CON)
			do_res_stat(Ind, A_DEX)
			do_res_stat(Ind, A_WIS)
			do_res_stat(Ind, A_INT)
			do_res_stat(Ind, A_CHR)
			restore_level(Ind)
			fire_ball(Ind, GF_RESTORE_PLAYER, 0, 2 + 4, 1, " recites a prayer.")
			end,
	["info"] = 	function()
			return ""
			end,
	["desc"] = 	{
			"Restores drained stats and lost experience.",
			"***Automatically projecting***",
	}
}

--[[ old mind focus spell
HSANITY = add_spell {
	["name"] = 	"Mind Focus",
	["school"] = 	{SCHOOL_HCURING},
	["am"] = 	75,
	["level"] = 	21,
	["mana"] = 	50,
	["mana_max"] = 	100,
	["fail"] = 	50,
	["stat"] = 	A_WIS,
	["spell"] = 	function()
			set_image(Ind, 0)
			if get_level(Ind, HSANITY, 50) >= 20 then
				if player.csane < (player.msane * 6 / 12) then
					player.csane = (player.msane * 6 / 12)
				end
				fire_ball(Ind, GF_SANITY_PLAYER, 0, 6 * 2, 1, " waves over your eyes, murmuring some words.")
			elseif get_level(Ind, HSANITY, 50) >= 10 then
				if player.csane < (player.msane * 3 / 12) then
					player.csane = (player.msane * 3 / 12)
				end
				fire_ball(Ind, GF_SANITY_PLAYER, 0, 3 * 2, 1, " waves over your eyes, murmuring some words.")
			else
				fire_ball(Ind, GF_SANITY_PLAYER, 0, 0, 1, " waves over your eyes.")
			end
			end,
	["info"] = 	function()
			return ""
			end,
	["desc"] = 	{
			"Frees your mind from hallucinations",
			"At level 10 it slightly cures very bad insanity",
			"At level 20 it fairly cures very bad insanity",
			"***Automatically projecting***",
		}
}
]]

-- the new mind focus spell - mikaelh
-- effect ranges from a light SN potion at level 21 to a serious SN potion at level 50
-- increased max_mana from 100 to 150
HSANITY = add_spell {
	["name"] = 	"Faithful Focus",
	["school"] = 	{SCHOOL_HCURING},
	["am"] = 	75,
	["level"] = 	21,
	["mana"] = 	50,
	["mana_max"] = 	50,
	["fail"] = 	50,
	["stat"] = 	A_WIS,
	["spell"] = 	function()
			set_afraid(Ind, 0)
			set_res_fear(Ind, get_level(Ind, HSANITY, 50))
			set_confused(Ind, 0)
			set_image(Ind, 0)

			heal_insanity(Ind, 15 + get_level(Ind, HSANITY, 55))
			if player.csane == player.msane then
				msg_print(Ind, "You are in full command of your mental faculties.")
			end

			fire_ball(Ind, GF_SANITY_PLAYER, 0, 30 + get_level(Ind, HSANITY, 110), 1, " waves over your eyes, murmuring some words..")
			end,
	["info"] = 	function()
			return "cures "..(15 + get_level(Ind, HSANITY, 55)).." SN"
			end,
	["desc"] = 	{
			"Frees your mind from fear, confusion, hallucinations",
			"and also cures some insanity.",
			"***Automatically projecting***",
		}
}

HRESURRECT = add_spell {
	["name"] = 	"Resurrection",
	["school"] = 	{SCHOOL_HCURING},
	["am"] = 	100,
	["level"] = 	30,
	["mana"] = 	200,
	["mana_max"] = 	200,
	["fail"] = 	50,
	["stat"] = 	A_WIS,
	["spell"] = 	function()
			fire_ball(Ind, GF_RESURRECT_PLAYER, 0, get_exp_loss(), 1, " resurrects you!")
			end,
	["info"] = 	function()
			return "exp -"..get_exp_loss().."%"
			end,
	["desc"] = 	{
			"Resurrects another player's ghost back to life.",
			"The higher the skill, the less experience he will lose.",
		}
}

HDELBB = add_spell {
	["name"] = 	"Soul Curing",
	["school"] = 	{SCHOOL_HCURING},
	["am"] = 	75,
	["level"] = 	25,	-- 45 the_sandman: too high lvl and this spell doesn't seem to be useful then. Asked around,
				-- and ppl say their first encounter with RW is about pvp 25-32ish.
	["mana"] = 	150,	-- was 200. Only chat/hope has priests with >200 mana at lvl ~25+ =)
	["mana_max"] = 	150,
	["fail"] = 	0,
	["stat"] = 	A_WIS,
	["spell"] = 	function()
			msg_print(Ind, "You feel a calming warmth touching your soul.");
			if (player.black_breath) then
				msg_print(Ind, "The hold of the Black Breath on you is broken!");
				player.black_breath = FALSE
			end
			fire_ball(Ind, GF_SOULCURE_PLAYER, 0, 2, 1, " chants loudly, praising the light!")
			end,
	["info"] = 	function()
			return ""
			end,
	["desc"] = 	{
			"Cures the Black Breath.",
			"***Automatically projecting***",
		}
}

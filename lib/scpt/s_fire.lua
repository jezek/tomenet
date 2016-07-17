-- handle the fire school

GLOBELIGHT_I = add_spell {
	["name"] = 	"Globe of Light I",
	["school"] = 	{SCHOOL_FIRE},
	["level"] = 	2,
	["mana"] = 	4,
	["mana_max"] = 	4,
	["fail"] = 	10,
	["spell"] = 	function()
		if get_level(Ind, GLOBELIGHT_I, 50) >= 10 then
			lite_area(Ind, 10, 4)
		else
			msg_print(Ind, "You are surrounded by a globe of light")
			lite_room(Ind, player.wpos, player.py, player.px)
		end
	end,
	["info"] = 	function()
			return ""
	end,
	["desc"] = 	{
			"Creates a globe of magical light.",
			"At level 10 it damages monsters that are susceptible to light.",
	}
}
GLOBELIGHT_II = add_spell {
	["name"] = 	"Globe of Light II",
	["school"] = 	{SCHOOL_FIRE},
	["level"] = 	22,
	["mana"] = 	15,
	["mana_max"] = 	15,
	["fail"] = 	10,
	["spell"] = 	function()
			msg_print(Ind, "You are surrounded by a globe of light")
			lite_room(Ind, player.wpos, player.py, player.px)
			fire_ball(Ind, GF_LITE, 0, 10 + get_level(Ind, GLOBELIGHT_I, 100), 5 + get_level(Ind, GLOBELIGHT_I, 6), " calls a globe of light for")
	end,
	["info"] = 	function()
			return "dam "..(10 + get_level(Ind, GLOBELIGHT_I, 100)).." rad "..(5 + get_level(Ind, GLOBELIGHT_I, 6))
	end,
	["desc"] = 	{ "Creates a globe of magical light, powerful enough to hurt monsters.", }
}

function get_firebolt_dam(Ind, limit_lev)
	--return 5 + get_level(Ind, FIREBOLT, 25), 7 + get_level(Ind, FIREBOLT, 25) + 1
	local lev

	lev = get_level(Ind, FIREBOLT_I, 50)
	if limit_lev ~= 0 and lev > limit_lev then lev = limit_lev + (lev - limit_lev) / 3 end

	return 5 + (lev / 2), 7 + (lev / 2) + 1
end
FIREBOLT_I = add_spell {
	["name"] = 	"Fire Bolt I",
	["school"] = 	SCHOOL_FIRE,
	["level"] = 	10,
	["mana"] = 	3,
	["mana_max"] = 	3,
	["fail"] = 	-10,
	["direction"] = TRUE,
	["ftk"] = 	1,
	["spell"] = 	function(args)
			fire_bolt(Ind, GF_FIRE, args.dir, damroll(get_firebolt_dam(Ind, 1)), " casts a fire bolt for")
	end,
	["info"] = 	function()
			local x, y

			x, y = get_firebolt_dam(Ind, 1)
			return "dam "..x.."d"..y
	end,
	["desc"] = 	{ "Conjures up fire into a powerful bolt.", }
}
FIREBOLT_II = add_spell {
	["name"] = 	"Fire Bolt II",
	["school"] = 	SCHOOL_FIRE,
	["level"] = 	25,
	["mana"] = 	6,
	["mana_max"] = 	6,
	["fail"] = 	-30,
	["direction"] = TRUE,
	["ftk"] = 	1,
	["spell"] = 	function(args)
			fire_bolt(Ind, GF_FIRE, args.dir, damroll(get_firebolt_dam(Ind, 15)), " casts a fire bolt for")
	end,
	["info"] = 	function()
			local x, y

			x, y = get_firebolt_dam(Ind, 15)
			return "dam "..x.."d"..y
	end,
	["desc"] = 	{ "Conjures up fire into a powerful bolt.", }
}
FIREBOLT_III = add_spell {
	["name"] = 	"Fire Bolt III",
	["school"] = 	SCHOOL_FIRE,
	["level"] = 	40,
	["mana"] = 	12,
	["mana_max"] = 	12,
	["fail"] = 	-60,
	["direction"] = TRUE,
	["ftk"] = 	1,
	["spell"] = 	function(args)
			fire_bolt(Ind, GF_FIRE, args.dir, damroll(get_firebolt_dam(Ind, 0)), " casts a fire bolt for")
	end,
	["info"] = 	function()
			local x, y

			x, y = get_firebolt_dam(Ind, 0)
			return "dam "..x.."d"..y
	end,
	["desc"] = 	{ "Conjures up fire into a powerful bolt.", }
}

FIREFLASH_I = add_spell {
	["name"] = 	"Fireflash I",
	["school"] = 	{SCHOOL_FIRE},
	["level"] = 	10,
	["mana"] = 	5,
	["mana_max"] = 	5,
	["fail"] = 	30,
	["direction"] = TRUE,
	["ftk"] = 	2,
	["spell"] = 	function(args)
			fire_ball(Ind, GF_FIRE, args.dir, 20 + get_level(Ind, FIREFLASH_I, 150), 2 + get_level(Ind, FIREFLASH_I, 3), " casts a fire ball for")
	end,
	["info"] = 	function()
		return "dam "..(20 + get_level(Ind, FIREFLASH_I, 150)).." rad "..(2 + get_level(Ind, FIREFLASH_I, 3))
	end,
	["desc"] = 	{ "Conjures a ball of fire to burn your foes to ashes.", }
}
FIREFLASH_II = add_spell {
	["name"] = 	"Fireflash II",
	["school"] = 	{SCHOOL_FIRE},
	["level"] = 	30,
	["mana"] = 	15,
	["mana_max"] = 	15,
	["fail"] = 	0,
	["direction"] = TRUE,
	["ftk"] = 	2,
	["spell"] = 	function(args)
			fire_ball(Ind, GF_HOLY_FIRE, args.dir, 20 + get_level(Ind, FIREFLASH_I, 300), 2 + get_level(Ind, FIREFLASH_I, 3), " casts a ball of holy fire for")
	end,
	["info"] = 	function()
		return "dam "..(20 + get_level(Ind, FIREFLASH_I, 300)).." rad "..(2 + get_level(Ind, FIREFLASH_I, 3))
	end,
	["desc"] = 	{ "Conjures a ball of holy fire to burn your foes to ashes.", }
}
FIREFLASH_III = add_spell {
	["name"] = 	"Fireflash III",
	["school"] = 	{SCHOOL_FIRE},
	["level"] = 	42,
	["mana"] = 	30,
	["mana_max"] = 	30,
	["fail"] = 	-45,
	["direction"] = TRUE,
	["ftk"] = 	2,
	["spell"] = 	function(args)
			fire_ball(Ind, GF_HOLY_FIRE, args.dir, 20 + get_level(Ind, FIREFLASH_I, 500), 2 + get_level(Ind, FIREFLASH_I, 3), " casts a ball of holy fire for")
	end,
	["info"] = 	function()
		return "dam "..(20 + get_level(Ind, FIREFLASH_I, 500)).." rad "..(2 + get_level(Ind, FIREFLASH_I, 3))
	end,
	["desc"] = 	{ "Conjures a ball of holy fire to burn your foes to ashes.", }
}

FIERYAURA_I = add_spell {
	["name"] = 	"Fiery Shield I",
	["school"] = 	{SCHOOL_FIRE},
	["level"] = 	16,
	["mana"] = 	25,
	["mana_max"] = 	25,
	["fail"] = 	50,
	["spell"] = 	function()
		local type
--		if (get_level(Ind, FIERYAURA, 50) >= 8) then
--			type = SHIELD_GREAT_FIRE
--		else
			type = SHIELD_FIRE
--		end
		set_shield(Ind, randint(20) + 10 + get_level(Ind, FIERYAURA_I, 30), 10, type, 5 + get_level(Ind, FIERYAURA_I, 10), 5 + get_level(Ind, FIERYAURA_I, 7))
	end,
	["info"] = 	function()
		return "dam "..(5 + get_level(Ind, FIERYAURA_I, 15)).."d"..(5 + get_level(Ind, FIERYAURA_I, 7)).." dur "..(10 + get_level(Ind, FIERYAURA_I, 30)).."+d20"
	end,
	["desc"] = 	{
			"Creates a shield of fierce flames around you."
--			,"At level 8 it turns into a greater kind of flame that can not be resisted"
	}
}
FIERYAURA_II = add_spell {
	["name"] = 	"Fiery Shield II",
	["school"] = 	{SCHOOL_FIRE},
	["level"] = 	36,
	["mana"] = 	50,
	["mana_max"] = 	50,
	["fail"] = 	15,
	["spell"] = 	function()
		local type
--		if (get_level(Ind, FIERYAURA, 50) >= 8) then
--			type = SHIELD_GREAT_FIRE
--		else
			type = SHIELD_FIRE
--		end
		set_shield(Ind, randint(20) + 10 + 35 + get_level(Ind, FIERYAURA_II, 35), 10, type, 5 + get_level(Ind, FIERYAURA_I, 10), 5 + get_level(Ind, FIERYAURA_I, 7))
	end,
	["info"] = 	function()
		return "dam "..(5 + get_level(Ind, FIERYAURA_I, 15)).."d"..(5 + get_level(Ind, FIERYAURA_I, 7)).." dur "..(10 + 35 + get_level(Ind, FIERYAURA_II, 35)).."+d20"
	end,
	["desc"] = 	{
			"Creates a shield of fierce flames around you."
--			,"At level 8 it turns into a greater kind of flame that can not be resisted"
	}
}

FIREWALL = add_spell {
	["name"] = 	"Firewall",
	["school"] = 	{SCHOOL_FIRE},
	["level"] = 	20,
	["mana"] = 	30,
	["mana_max"] = 	30,
	["fail"] = 	40,
	["direction"] = TRUE,
	["spell"] = 	function(args)
		fire_wall(Ind, GF_FIRE, args.dir, 20 + get_level(Ind, FIREWALL, 252), 6 + get_level(Ind, FIREWALL, 4), 8, " summons a firewall for")
	end,
	["info"] = 	function()
		return "dam "..(20 + get_level(Ind, FIREWALL, 252)).." dur "..(6 + get_level(Ind, FIREWALL, 4))
	end,
	["desc"] = 	{ "Creates a fiery wall to incinerate monsters stupid enough to move through it." }
}

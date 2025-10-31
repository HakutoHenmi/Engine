// AnimationJson.h
#pragma once
#include "Animation.h"
#include <nlohmann/json.hpp>

namespace Engine {

// ---- Event ----
inline void to_json(nlohmann::json& j, const AnimClip::Event& e) {
	j = {
	    {"t",     e.t    },
        {"label", e.label}
    };
}
inline void from_json(const nlohmann::json& j, AnimClip::Event& e) {
	j.at("t").get_to(e.t);
	j.at("label").get_to(e.label);
}

// ---- Keyframe ----
template<class T> inline void to_json(nlohmann::json& j, const AnimKeyframe<T>& k) {
	j = {
	    {"t",      k.t          },
        {"v",      k.v          },
        {"interp", (int)k.interp}
    };
}
template<class T> inline void from_json(const nlohmann::json& j, AnimKeyframe<T>& k) {
	j.at("t").get_to(k.t);
	j.at("v").get_to(k.v);
	int ip = 1; // Linear
	if (j.contains("interp"))
		j.at("interp").get_to(ip);
	k.interp = (Interp)ip;
}

// ---- Track ----
template<class T> inline void to_json(nlohmann::json& j, const AnimTrack<T>& tr) {
	j = {
	    {"name",    tr.name   },
        {"enabled", tr.enabled},
        {"keys",    tr.keys   }
    };
}
template<class T> inline void from_json(const nlohmann::json& j, AnimTrack<T>& tr) {
	if (j.contains("name"))
		j.at("name").get_to(tr.name);
	if (j.contains("enabled"))
		j.at("enabled").get_to(tr.enabled);
	if (j.contains("keys"))
		j.at("keys").get_to(tr.keys);
}

// ---- Clip ----
inline void to_json(nlohmann::json& j, const AnimClip& c) {
	j = {
	    {"name",   c.name  },
        {"length", c.length},
        {"loop",   c.loop  },
        {"posX",   c.posX  },
        {"posY",   c.posY  },
        {"posZ",   c.posZ  },
        {"rotX",   c.rotX  },
        {"rotY",   c.rotY  },
        {"rotZ",   c.rotZ  },
	    {"sclX",   c.sclX  },
        {"sclY",   c.sclY  },
        {"sclZ",   c.sclZ  },
        {"colR",   c.colR  },
        {"colG",   c.colG  },
        {"colB",   c.colB  },
        {"colA",   c.colA  },
        {"events", c.events}
    };
}

inline void from_json(const nlohmann::json& j, AnimClip& c) {
	j.at("name").get_to(c.name);
	j.at("length").get_to(c.length);
	j.at("loop").get_to(c.loop);

	j.at("posX").get_to(c.posX);
	j.at("posY").get_to(c.posY);
	j.at("posZ").get_to(c.posZ);
	j.at("rotX").get_to(c.rotX);
	j.at("rotY").get_to(c.rotY);
	j.at("rotZ").get_to(c.rotZ);
	j.at("sclX").get_to(c.sclX);
	j.at("sclY").get_to(c.sclY);
	j.at("sclZ").get_to(c.sclZ);
	j.at("colR").get_to(c.colR);
	j.at("colG").get_to(c.colG);
	j.at("colB").get_to(c.colB);
	j.at("colA").get_to(c.colA);

	if (j.contains("events"))
		j.at("events").get_to(c.events);
}

} // namespace Engine

project "winmm"
	uuid				"4a924b2a-622a-4f15-b933-0d55eb35d872"
	kind				"SharedLib"
	characterset		"Unicode"
	targetname			"winmm"
	language			"C++"
	cppdialect 			"C++20" 
	pchheader 			"pch.h"
	pchsource 			"src/pch.cpp"

	files
	{
		"src/**.inc",
		"src/**.asm",
		"src/**.def",
		"src/**.cpp",
		"src/**.hpp",
		"src/**.h",
	}	
	
	vpaths {
		["src/*"]					= { "winmm", "src" },
	}
	
	includedirs {
		"src",
	}
	
	libdirs {
		"lib/",
	}
	
	links {
		
	}
	
	buildoptions {
		
	}

	defines {

	}
		
	filter "configurations:Debug"
		defines { "DEBUG" }
		optimize "Off"
		symbols "On"
		linktimeoptimization "Off"
		
	filter "configurations:Dev"
		flags { "NoIncrementalLink" }
		optimize "Off"
		symbols "Off"
		linktimeoptimization "On"
		
	filter "configurations:Release"
		flags { "NoIncrementalLink" }
		defines { "NDEBUG" }
		optimize "Full"
		symbols "Off"
		linktimeoptimization "On"
		rtti "Off"

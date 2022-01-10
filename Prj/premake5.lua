include "common.lua"

solution "iDebugTool"
    project "iDebugTool"
        kind "WindowedApp"

        files
        {
            "../Src/**.h",
            "../Src/**.cpp",
            "../Src/**.ui",
        }
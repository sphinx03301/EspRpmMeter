
file(GLOB SRCS ./*.cpp ./*.c LovyanGFX/src/lgfx/*.cpp  LovyanGFX/src/lgfx/platforms/*.cpp  
    LovyanGFX/src/lgfx/utility/*.c  LovyanGFX/src/Fonts/*.cpp  LovyanGFX/src/Fonts/IPA/*.c)
set(COMPONENT_SRCS ${SRCS})

set(COMPONENT_ADD_INCLUDEDIRS
	.
    LovyanGFX/src
    LovyanGFX/src/lgfx
    LovyanGFX/src/lgfx/platforms
    LovyanGFX/src/lgfx/utility
    LovyanGFX/src/Fonts
    )
register_component()


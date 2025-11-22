module;

export module AlmondShell.aengine:renderers;

#ifdef ALMOND_USING_SDL
export import "asdlcontext.hpp";
#endif
#ifdef ALMOND_USING_SFML
export import "asfmlcontext.hpp";
#endif
#ifdef ALMOND_USING_SOFTWARE_RENDERER
export import "asoftrenderer_context.hpp";
#endif
#ifdef ALMOND_USING_RAYLIB
export import "araylibcontext.hpp";
#endif

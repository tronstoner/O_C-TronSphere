// hemisphere_config_custom.h -- applet registry for CUSTOM_BUILD envs.
//
// Included from INSIDE the `AppletRegistry reg{ ... }` initializer in
// firmware/software/src/hemisphere_config.h. Each line is one
// DeclareApplet<Class>{id, categories}, followed by a comma.
//
// Rules:
//  - Our applets use ids 200+. Never reuse or renumber an id (presets store it).
//  - Upstream applets are opted out by default. To add one, add its line here
//    with its upstream id (copy from hemisphere_config.h), one at a time.
//  - The applet's header must be included in CUSTOM_APPLET_INCLUDES below
//    (upstream headers are already included by hemisphere_config.h).
#ifdef CUSTOM_APPLET_INCLUDES
#include "custom/applets/Hello.h"
#include "custom/applets/Likembe.h"
#else
    DeclareApplet<Hello>{200, 0x80},
    DeclareApplet<Likembe>{201, 0x02},
    // Upstream opt-ins (example, disabled):
    // DeclareApplet<Voltage>{43, 0x10},
#endif

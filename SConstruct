#!/usr/bin/env python

target_path = ARGUMENTS.pop("target_path", "demo/addons/godot-scene-synchronizer/bin/")
target_name = ARGUMENTS.pop("target_name", "libscenesynchronizer")

env = SConscript("godot-cpp/SConstruct")

env_vars = Variables()
env_vars.Update(env)
Help(env_vars.GenerateHelpText(env))

target = "{}{}".format(
    target_path, target_name
)

# For reference:
# - CCFLAGS are compilation flags shared between C and C++
# - CFLAGS are for C-specific compilation flags
# - CXXFLAGS are for C++-specific compilation flags
# - CPPFLAGS are for pre-processor flags
# - CPPDEFINES are for pre-processor defines
# - LINKFLAGS are for linking flags

# tweak this if you want to use different folders, or more folders, to store your source code in.
env.Append(CPPPATH=["src/"])
sources = Glob("src/*.cpp")

if env["target"] in ["editor", "template_debug"]:
    doc_data = env.GodotCPPDocData("src/gen/doc_data.gen.cpp", source=Glob("doc_classes/*.xml"))
    sources.append(doc_data)

if env["platform"] == "macos":
    library = env.SharedLibrary(
        "{}.{}.{}.framework/{}.{}.{}".format(
            target,
            env["platform"],
            env["target"],
            target_name,
            env["platform"],
            env["target"]
        ),
        source=sources,
    )
elif env["platform"] == "ios":
    if env["ios_simulator"]:
        library = env.StaticLibrary(
            "{}.{}.{}.simulator.a".format(
                target,
                env["platform"],
                env["target"]),
            source=sources,
        )
    else:
        library = env.StaticLibrary(
            "{}.{}.{}.a".format(
                target,
                env["platform"],
                env["target"]),
            source=sources,
        )
else:
    library = env.SharedLibrary(
        "{}{}{}".format(
            target,
            env["suffix"],
            env["SHLIBSUFFIX"]
        ),
        source=sources,
    )

Default(library)

#!/usr/bin/env python

use_instrumentation = str(ARGUMENTS.pop("instrumentation", "false")).lower() in ["true", "1", "yes"]
instrumentation_threshold = float(ARGUMENTS.pop("instrumentation_threshold", 0))

target_path = ARGUMENTS.pop("target_path", "demo/addons/godot-scene-synchronizer/bin/")
target_name = ARGUMENTS.pop("target_name", "libscenesynchronizer")

env = SConscript("godot-cpp/SConstruct")

if use_instrumentation:
    env.Append(CPPDEFINES=["INSTRUMENTATION_ENABLED"])
    env.Append(CPPDEFINES=[("INSTRUMENTATION_THRESHOLD_MS", instrumentation_threshold)])
    print(f"Building with instrumentation (Threshold MS: {instrumentation_threshold})")

target = "{}{}".format(
    target_path, target_name
)

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

There are the sources for SuperSU's **suhide-lite** mod.

At time of writing, these correspond to **v1.09**.

Sources for the (uninteresting) Java parts can be found in **suhide/src**.

Sources for the (interesting) C parts can be found in **suhide/native**.

The (release) build process will output files to **ZIP/suhide**.

The folder structure is like this because it is copied out of the larger build tree used for SuperSU builds. The script that builds the actual flashable ZIP from the **ZIP/suhide** folder is missing as it depends on internal tools.

The gradle build script depends on an (included) older Gradle version and handles building with the NDK manually. 

Your *local.properties* file needs correct paths set for both *sdk.dir* as well as *ndk.dir* variables.

Builds have been tested with NDK **r10e** specifically, on a Windows machine.


Published with permission from CCMT

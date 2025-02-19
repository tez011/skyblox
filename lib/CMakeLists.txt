cmake_minimum_required(VERSION 3.21)

# ---- external dependencies ----
CPMAddPackage(NAME cglm
              VERSION 0.9.3
              GIT_REPOSITORY https://github.com/recp/cglm.git
              EXCLUDE_FROM_ALL TRUE
              OPTIONS "CGLM_SHARED OFF"
                      "CGLM_STATIC ON")
CPMAddPackage(NAME cjson
              VERSION 1.7.18
              GIT_REPOSITORY https://github.com/DaveGamble/cJSON.git
              EXCLUDE_FROM_ALL TRUE
              OPTIONS "BUILD_SHARED_LIBS OFF")
CPMAddPackage(NAME gl3w
              VERSION 1.0
              GIT_REPOSITORY https://github.com/skaslev/gl3w.git
              GIT_TAG 3a33275633ce4be433332dc776e6a5b3bdea6506
              EXCLUDE_FROM_ALL TRUE)
CPMAddPackage(NAME nuklear
              VERSION 4.12.4
              GIT_REPOSITORY https://github.com/Immediate-Mode-UI/Nuklear.git
              GIT_TAG 4.12.4
              EXCLUDE_FROM_ALL TRUE)
CPMAddPackage(NAME physicsfs
              VERSION 3.2.0
              GIT_REPOSITORY https://github.com/icculus/physfs.git
              GIT_TAG release-3.2.0
              EXCLUDE_FROM_ALL TRUE
              OPTIONS "PHYSFS_ARCHIVE_ZIP TRUE"
                      "PHYSFS_ARCHIVE_7Z TRUE"
                      "PHYSFS_ARCHIVE_GRP FALSE"
                      "PHYSFS_ARCHIVE_WAD FALSE"
                      "PHYSFS_ARCHIVE_HOG FALSE"
                      "PHYSFS_ARCHIVE_MVL FALSE"
                      "PHYSFS_ARCHIVE_QPAK FALSE"
                      "PHYSFS_ARCHIVE_SLB FALSE"
                      "PHYSFS_ARCHIVE_ISO9660 TRUE"
                      "PHYSFS_ARCHIVE_VDF FALSE"
                      "PHYSFS_BUILD_TEST FALSE"
                      "PHYSFS_BUILD_DOCS FALSE"
                      "PHYSFS_BUILD_STATIC FALSE"
                      "PHYSFS_DISABLE_INSTALL ON"
                      "PHYSFS_TARGETNAME_UNINSTALL PHYSFS_UNINSTALL_TGT_INT")
CPMAddPackage(NAME stb
              VERSION 2.29
              GIT_REPOSITORY https://github.com/nothings/stb.git
              GIT_TAG 0bc88af4de5fb022db643c2d8e549a0927749354
              DOWNLOAD_ONLY YES)
CPMAddPackage(NAME tinycthread
              VERSION 1.1
              GIT_REPOSITORY https://github.com/tinycthread/tinycthread.git
              EXCLUDE_FROM_ALL TRUE)

# ---- custom targets ----
if (cjson_ADDED)
    target_include_directories(cjson PUBLIC $<BUILD_INTERFACE:${cjson_SOURCE_DIR}>)
endif()
if (nuklear_ADDED)
    add_library(nuklear INTERFACE)
    get_filename_component(SDL2_INCLUDE_PARENT "${SDL2_INCLUDE_DIRS}" DIRECTORY)
    target_include_directories(nuklear INTERFACE $<BUILD_INTERFACE:${nuklear_SOURCE_DIR}> 
                                                 $<BUILD_INTERFACE:${SDL2_INCLUDE_PARENT}>)
endif()
if (stb_ADDED)
    add_library(stb INTERFACE)
    target_include_directories(stb INTERFACE $<BUILD_INTERFACE:${stb_SOURCE_DIR}>)
endif()
if (tinycthread_ADDED)
    add_library(tinycthread STATIC EXCLUDE_FROM_ALL ${tinycthread_SOURCE_DIR}/source/tinycthread.c)
    target_include_directories(tinycthread PUBLIC $<BUILD_INTERFACE:${tinycthread_SOURCE_DIR}/source>)
endif()
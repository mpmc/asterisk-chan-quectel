#!/bin/bash -e

echoerr() { echo "$@" 1>&2; }

# apt install dpkg-dev
readonly JQ_DEB_SCRIPT='
{
    version: 3, cmakeMinimumRequired: {major: 3,minor: 22, patch: 1},
    configurePresets: [
        {
            name: "deb-internal",
            hidden: true,
            environment: {
                CFLAGS: "\(env.CFLAGS) \(env.CPPFLAGS)",
                CXXFLAGS: "\(env.CXXFLAGS) \(env.CPPFLAGS)",
                LDFLAGS: env.LDFLAGS
            }
        },
        {
            name: "deb",
            inherits: ["deb-internal", "default"]
        }
    ],
    buildPresets: [
        {
            name: "deb",
            inherits: "default",
            configurePreset: "deb"
        },
        {
            name: "package-deb",
            configurePreset: "deb",
            jobs: 0,
            cleanFirst: true,
            targets: "package"
        }
    ]
}'

readonly JQ_RPI_SCRIPT='
{
    version: 3, cmakeMinimumRequired: {major: 3,minor: 22, patch: 1},
    configurePresets: [
        {
            name: "deb-internal",
            hidden: true,
            cacheVariables: {
                "CPACK_DEBIAN_PACKAGE_SHLIBDEPS": {
                    type: "BOOL",
                    value: true
                },
                "CPACK_DEBIAN_PACKAGE_GENERATE_SHLIBS_POLICY": {
                    type: "STRING",
                    value: ">="
                },
                "CPACK_DEBIAN_PACKAGE_SHLIBDEPS_PRIVATE_DIRS": {
                    type: "STRING",
                    value: "/build/rpi/usr/local/lib/arm-linux-gnueabihf;/build/rpi/usr/local/lib;/build/rpi/usr/lib/arm-linux-gnueabihf;/build/rpi/usr/lib"
                }
            },
            environment: {
                CFLAGS: "\(env.CFLAGS) \(env.CPPFLAGS)",
                CXXFLAGS: "\(env.CXXFLAGS) \(env.CPPFLAGS)",
                LDFLAGS: env.LDFLAGS
            }
        },
        {
            name: "deb",
            inherits: ["deb-internal", "default"]
        }
    ],
    buildPresets: [
        {
            name: "deb",
            inherits: "default",
            configurePreset: "deb",
            inheritConfigureEnvironment: true
        },
        {
            name: "package-deb",
            configurePreset: "deb",
            jobs: 0,
            cleanFirst: true,
            targets: "package",
            inheritConfigureEnvironment: true
        }
    ]
}'

# yum install rpm-build
readonly JQ_RPM_SCRIPT='
{
    version: 3, cmakeMinimumRequired: {major: 3,minor: 22, patch: 1},
    configurePresets: [
        {
            name: "rpm-internal",
            hidden: true,
            environment: {
                CFLAGS: env.CFLAGS,
                LDFLAGS: env.LDFLAGS
            }
        },
        {
            name: "rpm",
            inherits: ["rpm-internal", "default"]
        }
    ],
    buildPresets: [
        {
            name: "rpm",
            inherits: "default",
            configurePreset: "rpm"
        },
        {
            name: "package-rpm",
            configurePreset: "rpm",
            jobs: 0,
            cleanFirst: true,
            targets: "package"
        }
    ]
}'

case $1 in
    deb)
    eval "$(env 'DEB_BUILD_OPTIONS=reproducible=-fixfilepath,-fixdebugpath' dpkg-buildflags --export=sh)"
    jq -n "$JQ_DEB_SCRIPT"
    ;;

    rpi)
    eval "$(env 'DEB_BUILD_OPTIONS=reproducible=-fixfilepath,-fixdebugpath' dpkg-buildflags --export=sh)"
    jq -n "$JQ_RPI_SCRIPT"    
    ;;

    rpm)
    env "CFLAGS=$(rpm -E '%optflags')" "LDFLAGS=$(rpm -E '%__global_ldflags')" jq -n "$JQ_RPM_SCRIPT"
    ;;

    *)
    echoerr Unknown package manager
    exit 1;;
esac

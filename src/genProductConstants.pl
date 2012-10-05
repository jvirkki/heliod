#
# DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS HEADER.
#
# Copyright 2008 Sun Microsystems, Inc. All rights reserved.
#
# THE BSD LICENSE
#
# Redistribution and use in source and binary forms, with or without 
# modification, are permitted provided that the following conditions are met:
#
# Redistributions of source code must retain the above copyright notice, this
# list of conditions and the following disclaimer. 
# Redistributions in binary form must reproduce the above copyright notice, 
# this list of conditions and the following disclaimer in the documentation 
# and/or other materials provided with the distribution. 
#
# Neither the name of the  nor the names of its contributors may be
# used to endorse or promote products derived from this software without 
# specific prior written permission. 
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER 
# OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, 
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; 
# OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, 
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR 
# OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF 
# ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

unless (@ARGV) {
    die "Usage: $0 <input-file>  \n".
        "Where input file will contain list of constants as key=value \n";
}

($inputfile) = (@ARGV);
chomp $inputfile;
local *IN;

unless (open(IN,"<$inputfile")) {
    close(IN);
    print STDERR "Unable to open input file: $inputfile, Reason:$!\n";
    exit 1;
}
while (<IN>) {
    next unless ($_);
    unless (/^(\w+)=(.*)$/) {
	    print STDERR "Parameter \"$_\" ignored (must be name-value pair).\n";
	    next;
    }
    # print "n = <$1>, v = <$2>\n";
    eval "\$$1=\"$2\"";
}
close(IN);

@MissingParameters;

# check required information
push @MissingParameters, "PRODUCT_ID"             unless defined ${PRODUCT_ID};
push @MissingParameters, "PRODUCT_HEADER_ID"      unless defined ${PRODUCT_HEADER_ID};
push @MissingParameters, "PRODUCT_VERSION"        unless defined ${PRODUCT_VERSION};
push @MissingParameters, "PRODUCT_FULL_VERSION"   unless defined ${PRODUCT_FULL_VERSION};
push @MissingParameters, "MajorVersion"           unless defined ${MajorVersion};
push @MissingParameters, "MinorVersion"           unless defined ${MinorVersion};
push @MissingParameters, "Platform"               unless defined ${Platform};
push @MissingParameters, "Architecture"           unless defined ${Architecture};
push @MissingParameters, "Security"               unless defined ${Security};

push @MissingParameters, "PRODUCT_MAGNUS_CONF"    unless defined ${PRODUCT_MAGNUS_CONF};
push @MissingParameters, "PRODUCT_WATCHDOG_BIN"   unless defined ${PRODUCT_WATCHDOG_BIN};
push @MissingParameters, "PRODUCT_DAEMON_BIN"     unless defined ${PRODUCT_DAEMON_BIN};
push @MissingParameters, "PRODUCT_I18N_DB"        unless defined ${PRODUCT_I18N_DB};
push @MissingParameters, "PRODUCT_ADMSERV_NAME"   unless defined ${PRODUCT_ADMSERV_NAME};

push @MissingParameters, "PRODUCT_PUBLIC_BIN_SUBDIR" unless defined ${PRODUCT_PUBLIC_BIN_SUBDIR};
push @MissingParameters, "PRODUCT_PRIVATE_BIN_SUBDIR" unless defined ${PRODUCT_PRIVATE_BIN_SUBDIR};
push @MissingParameters, "PRODUCT_LIB_SUBDIR"     unless defined ${PRODUCT_LIB_SUBDIR};
push @MissingParameters, "PRODUCT_JAR_SUBDIR"     unless defined ${PRODUCT_JAR_SUBDIR};
push @MissingParameters, "PRODUCT_RES_SUBDIR"     unless defined ${PRODUCT_RES_SUBDIR};
push @MissingParameters, "PRODUCT_DTD_SUBDIR"     unless defined ${PRODUCT_DTD_SUBDIR};
push @MissingParameters, "PRODUCT_TLD_SUBDIR"     unless defined ${PRODUCT_TLD_SUBDIR};
push @MissingParameters, "PRODUCT_ICONS_SUBDIR"   unless defined ${PRODUCT_ICONS_SUBDIR};
push @MissingParameters, "PRODUCT_INCLUDE_SUBDIR" unless defined ${PRODUCT_INCLUDE_SUBDIR};
push @MissingParameters, "PRODUCT_JDK_SUBDIR"     unless defined ${PRODUCT_JDK_SUBDIR};
push @MissingParameters, "PRODUCT_PERL_SUBDIR"    unless defined ${PRODUCT_PERL_SUBDIR};
push @MissingParameters, "PRODUCT_SNMP_SUBDIR"    unless defined ${PRODUCT_SNMP_SUBDIR};
push @MissingParameters, "PRODUCT_INSTALL_SUBDIR"     unless defined ${PRODUCT_INSTALL_SUBDIR};
push @MissingParameters, "PRODUCT_WEBAPP_SUBDIR"      unless defined ${PRODUCT_WEBAPP_SUBDIR};
push @MissingParameters, "PRODUCT_PLUGINS_SUBDIR"     unless defined ${PRODUCT_PLUGINS_SUBDIR};
push @MissingParameters, "PRODUCT_EXTRAS_SUBDIR"      unless defined ${PRODUCT_EXTRAS_SUBDIR};
push @MissingParameters, "PRODUCT_SAMPLES_SUBDIR"     unless defined ${PRODUCT_SAMPLES_SUBDIR};

#push @MissingParameters, ""     unless defined ${};

push @MissingParameters, "PLATFORM_JES_LIB_SUBDIR"         unless defined ${PLATFORM_JES_LIB_SUBDIR};
push @MissingParameters, "PLATFORM_JES_BINPATH"         unless defined ${PLATFORM_JES_BINPATH};
push @MissingParameters, "PLATFORM_JES_LIBPATH"         unless defined ${PLATFORM_JES_LIBPATH};
push @MissingParameters, "PLATFORM_JES_LIBPATH_64"       unless defined ${PLATFORM_JES_LIBPATH_64};
push @MissingParameters, "PLATFORM_JES_SERVER_CP"     unless defined ${PLATFORM_JES_SERVER_CP};
push @MissingParameters, "PLATFORM_JES_ADMIN_CP"     unless defined ${PLATFORM_JES_ADMIN_CP};
push @MissingParameters, "PLATFORM_JES_CLI_CP"     unless defined ${PLATFORM_JES_CLI_CP};
push @MissingParameters, "PLATFORM_JES_ANT_CP"     unless defined ${PLATFORM_JES_ANT_CP};
push @MissingParameters, "PLATFORM_JES_MFWK_HOME"     unless defined ${PLATFORM_JES_MFWK_HOME};

if (scalar @MissingParameters > 0) {
    print STDERR "Missing parameters: ", join (", ", @MissingParameters), ".\n";
    print STDERR "Please add definitions.\n";
    exit 1;
}

@date = localtime();

mkdir("${internal_root}/include", 0755);
$java_root = "${internal_root}/java";
mkdir("${java_root}", 0755);
mkdir("${java_root}/com", 0755);
mkdir("${java_root}/com/sun", 0755);
mkdir("${java_root}/com/sun/web", 0755);

if(!open(DEFINES_H, ">${internal_root}/include/definesEnterprise.h")) {
    print "can't open file: $!\n";
    exit 2;
}

if(!open(DEFINES_JAVA, ">${java_root}/com/sun/web/ProductConstants.java")) {
    print "can't open file: $!\n";
    exit 3;
}

if(!open(DEFINES_PROPS, ">${java_root}/ProductConstants.properties")) {
    print "can't open file: $!\n";
    exit 4;
}

# get the java version string from the jdk used in the build
$javacmd = "$jdk_dir/bin/java";
# flip slashes on windows
if (-d "\\") {
    $javacmd =~ s|/|\\|g;
}

if(`$javacmd -version 2>&1` =~ /"([^"]+)"/) {
    $PLATFORM_JAVA_VERSION = $1;
}

print_header();
print_comment("Build number");
$val_str = sprintf("%02d/%02d/%04d %02d:%02d",
		$date[4]+1, $date[3], $date[5] + 1900, $date[2], $date[1]);
print_str("BUILD_NUM", ${val_str});
print_comment("NT resource defs");
$val_str = sprintf("%04d,%02d,%02d,%02d", $date[5] + 1900, $date[4]+1, $date[3], $date[2]);
print_h("IWS_FILEVERSION", ${val_str});
$val_str = "${MajorVersion},0,0,${MinorVersion}";
print_h("IWS_PRODUCTVERSION", ${val_str});

print_comment("Optional features");
print_boolean("FEAT_MULTIPROCESS", ${FEAT_MULTIPROCESS});
print_boolean("FEAT_INTERNAL_LOG_ROTATION", ${FEAT_INTERNAL_LOG_ROTATION});
print_boolean("FEAT_NOLIMITS", ${FEAT_NOLIMITS});
print_boolean("FEAT_TUNEABLE", ${FEAT_TUNEABLE});
print_boolean("FEAT_DAEMONSTATS", ${FEAT_DAEMONSTATS});
print_boolean("FEAT_CLUSTER", ${FEAT_CLUSTER});
print_boolean("FEAT_DYNAMIC_GROUPS", ${FEAT_DYNAMIC_GROUPS});
print_boolean("FEAT_PASSWORD_POLICIES", ${FEAT_PASSWORD_POLICIES});
print_boolean("FEAT_UPGRADE", ${FEAT_UPGRADE});
print_boolean("FEAT_PKCS_MODULES", ${FEAT_PKCS_MODULES});
print_boolean("FEAT_PAM", ${FEAT_PAM});
print_boolean("FEAT_GSS", ${FEAT_GSS});
print_boolean("FEAT_SEARCH", ${FEAT_SEARCH});
print_boolean("FEAT_SNMP", ${FEAT_SNMP});
print_boolean("FEAT_L10N", ${FEAT_L10N});
print_boolean("FEAT_SMF", ${FEAT_SMF});

print_comment("Misc build related definitions");
print_str("BUILD_SECURITY", ${Security});
print_str("BUILD_PLATFORM", ${Platform});
print_str("BUILD_ARCH", ${Architecture});

# PumpkinAge is optional, if present, it writes out a PUMPKIN_HOUR which
# enables a time bomb on the server.
$val_str = sprintf("%d", time + ($PumpkinAge * 24 * 60 * 60));
print_h("PUMPKIN_HOUR", $val_str) if defined ${PumpkinAge};

print_str("PRODUCT_BRAND_NAME", "${BRAND_NAME} ${PRODUCT_NAME}");
print_str("PRODUCT_ID", ${PRODUCT_ID});
print_str("PRODUCT_HEADER_ID", ${PRODUCT_HEADER_ID});
print_str("PRODUCT_VERSION_ID", ${PRODUCT_VERSION});
print_str("PRODUCT_FULL_VERSION_ID", ${PRODUCT_FULL_VERSION});
print_str("PRODUCT_MAJOR_VERSION_STR", ${MajorVersion});
print_int("PRODUCT_MAJOR_VERSION", ${MajorVersion});
print_str("PRODUCT_MINOR_VERSION_STR", ${MinorVersion});
print_int("PRODUCT_MINOR_VERSION", ${MinorVersion});

# add these only for service packs
if (defined ${SPVersion} && length(${SPVersion})) {
    print_str("PRODUCT_SP_VERSION_STR", ${SPVersion});
    print_int("PRODUCT_SP_VERSION", ${SPVersion});
}

print_str("PRODUCT_PREVIEW_VERSION", ${PreviewVersion});

# file name and file layout related defines
print_comment("File name and file layout related definitions");
print_str("PRODUCT_MAGNUS_CONF", ${PRODUCT_MAGNUS_CONF});
print_str("PRODUCT_WATCHDOG_BIN", ${PRODUCT_WATCHDOG_BIN});
print_str("PRODUCT_DAEMON_BIN", ${PRODUCT_DAEMON_BIN});
print_str("PRODUCT_I18N_DB", ${PRODUCT_I18N_DB});
print_str("PRODUCT_ADMSERV_NAME", ${PRODUCT_ADMSERV_NAME});
print_str("PRODUCT_PLATFORM_SUBDIR", ${PRODUCT_PLATFORM_SUBDIR});
print_str("PRODUCT_PLATFORM_64_SUBDIR", ${PRODUCT_PLATFORM_64_SUBDIR});
print_str("PRODUCT_PUBLIC_BIN_SUBDIR", ${PRODUCT_PUBLIC_BIN_SUBDIR});
print_str("PRODUCT_PRIVATE_BIN_SUBDIR", ${PRODUCT_PRIVATE_BIN_SUBDIR});
print_str("PRODUCT_LIB_SUBDIR", ${PRODUCT_LIB_SUBDIR});
print_str("PRODUCT_JAR_SUBDIR", ${PRODUCT_JAR_SUBDIR});
print_str("PRODUCT_RES_SUBDIR", ${PRODUCT_RES_SUBDIR});
print_str("PRODUCT_DTD_SUBDIR", ${PRODUCT_DTD_SUBDIR});
print_str("PRODUCT_TLD_SUBDIR", ${PRODUCT_TLD_SUBDIR});
print_str("PRODUCT_ICONS_SUBDIR", ${PRODUCT_ICONS_SUBDIR});
print_str("PRODUCT_INCLUDE_SUBDIR", ${PRODUCT_INCLUDE_SUBDIR});
print_str("PRODUCT_JDK_SUBDIR", ${PRODUCT_JDK_SUBDIR});
print_str("PRODUCT_PERL_SUBDIR", ${PRODUCT_PERL_SUBDIR});
print_str("PRODUCT_SNMP_SUBDIR", ${PRODUCT_SNMP_SUBDIR});
print_str("PRODUCT_INSTALL_SUBDIR", ${PRODUCT_INSTALL_SUBDIR});
print_str("PRODUCT_WEBAPP_SUBDIR", ${PRODUCT_WEBAPP_SUBDIR});
print_str("PRODUCT_PLUGINS_SUBDIR", ${PRODUCT_PLUGINS_SUBDIR});
print_str("PRODUCT_EXTRAS_SUBDIR", ${PRODUCT_EXTRAS_SUBDIR});
print_str("PRODUCT_SAMPLES_SUBDIR", ${PRODUCT_SAMPLES_SUBDIR});

print_str("PLATFORM_JES_LIB_SUBDIR", ${PLATFORM_JES_LIB_SUBDIR});
print_str("PLATFORM_JES_BINPATH", ${PLATFORM_JES_BINPATH});
print_str("PLATFORM_JES_LIBPATH", ${PLATFORM_JES_LIBPATH});
print_str("PLATFORM_JES_LIBPATH_64", ${PLATFORM_JES_LIBPATH_64});

print_str("PLATFORM_JES_SERVER_CP", ${PLATFORM_JES_SERVER_CP});
print_str("PLATFORM_JES_ADMIN_CP", ${PLATFORM_JES_ADMIN_CP});
print_str("PLATFORM_JES_CLI_CP", ${PLATFORM_JES_CLI_CP});
print_str("PLATFORM_JES_ANT_CP", ${PLATFORM_JES_ANT_CP});
print_str("PLATFORM_JES_MFWK_HOME", ${PLATFORM_JES_MFWK_HOME});
print_str("PLATFORM_JES_PATCH_ID", ${PLATFORM_JES_PATCH_ID});
print_str("PLATFORM_JAVA_VERSION", ${PLATFORM_JAVA_VERSION});


print_footer();

close (DEFINES_H);
close (DEFINES_PROPS);
close (DEFINES_JAVA);

exit 0;

sub print_header {
    print DEFINES_H "/*\n * This file is automatically generated.\n * Please do not edit this file manually.\n * It contains information about this build.\n */\n#if !defined (DEFINESENTERPRISE_H)\n#define DEFINESENTERPRISE_H\n";
    print DEFINES_JAVA "package com.sun.web;\n\npublic class ProductConstants {\n";
    print DEFINES_PROPS <<EOF;
#
# Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
EOF

}

sub print_footer {
    print DEFINES_H "\n#endif /* !defined(DEFINESENTERPRISE_H) */\n\n";
    print DEFINES_PROPS "\n\n";
    print DEFINES_JAVA "}\n\n";
}

sub print_comment {
    $comment_str = $_[0];
    print DEFINES_H "\n/* ${comment_str} */\n";
    print DEFINES_PROPS "\n# ${comment_str}\n";
    print DEFINES_JAVA "\n\t// ${comment_str}\n";
}

sub print_int {
    print_h($_[0], $_[1]);
    print_props($_[0], $_[1]);
    print_java_int($_[0], $_[1]);
}

sub print_boolean {
    if ($_[1]) {
        print_h_boolean($_[0]);
        print_props($_[0], "true");
        print_java_boolean($_[0], "true");
    } else {
        print_java_boolean($_[0], "false");
    }
}

sub print_str {
    my($val_str) = "\"$_[1]\"";
    print_h($_[0], ${val_str});
    print_props($_[0], $_[1]);
    print_java_string($_[0], ${val_str});
}

sub print_h {
    print DEFINES_H "#define $_[0] $_[1]\n";
    return;
}

sub print_h_boolean {
    print DEFINES_H "#define $_[0]\n";
    return;
}

sub print_props {
    print DEFINES_PROPS "$_[0]=$_[1]\n";
    return;
}

sub print_java_int {
    print DEFINES_JAVA "\tpublic static final int $_[0]=$_[1];\n";
    return;
}

sub print_java_boolean {
    print DEFINES_JAVA "\tpublic static final boolean $_[0]=$_[1];\n";
    return;
}

sub print_java_string {
    print DEFINES_JAVA "\tpublic static final String $_[0]=$_[1];\n";
    return;
}


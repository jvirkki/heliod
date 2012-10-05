#!/usr/bin/perl

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

use Cwd;

my %hash = ();
foreach $n (0 .. $#ARGV) 
{
   $arg = @ARGV[$n];
   @arr =  split(/=/, $arg);
   $hash{ $arr[0] } = $arr[1]; 
}

# Convert the install & instance directory locations to absolute paths
my $dir=$hash{'WS_INSTALL_ROOT'};
$hash{'WS_INSTALL_ROOT'} = Cwd::abs_path($dir);
$dir=$hash{'WS_INSTANCE_ROOT'};
$hash{'WS_INSTANCE_ROOT'} = Cwd::abs_path($dir);

if (exists $hash{'INPUT_DIR'})  {
} else {
    die "Please pass INPUT_DIR to the script\n";
}
if (exists $hash{'OUTPUT_DIR'}) { 
}
else {
    die "Please pass OUTPUT_DIR to the script\n";
}

$input_dir = $hash{'INPUT_DIR'};
$output_dir = $hash{'OUTPUT_DIR'};

opendir(DIR, $input_dir) || die "can't opendir $input_dir: $!\n";
@files =  readdir(DIR);
closedir DIR;

foreach $file (@files)
{
    next if ($file eq "." or $file eq "..");

    my $inputFile = join("/", $input_dir, $file);
    open(IN, "<", $inputFile) or die "Unable to open file $inputFile $!\n";
    print "Reading file $inputFile\n";
    @lines = <IN>;
    close(IN);

    $file =~ s/\.template//;
    my $outputFile = join("/", $output_dir, $file);
    open(OUT, ">" , $outputFile) or die "Unable to open file $outputFile $!\n";
    print "Writing to $outputFile\n";
    foreach $line (@lines)
    {
         while (my ($key, $value) = each(%hash) )  {
             $line =~ s/%%%$key%%%/$value/g;
         }
         print OUT $line;
    }
    close(OUT);
}

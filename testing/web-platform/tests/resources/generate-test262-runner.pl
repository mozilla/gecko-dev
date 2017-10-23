#!/usr/bin/env perl

# Diego Pino Garcia <dpino@igalia.com>
#
# Helper script to generate a web-platform test feeded with several test262's tests.
# The test runs the test262's test inside several agents (DedicatedWorker,
# SharedWorker and ServiceWorker).

use strict;
use warnings;

my $TEMPLATE = "test262-runner.template.html";
my $OUTPUT = "../atomics/test262-runner.html";

sub readfile {
   my $filename = shift;
   my @ret = ();
   open my $fh, "<", $filename or die "Could not open $filename: $!";
   while (<$fh>) {
      push @ret, $_;
   }
   close $fh;
   return join "", @ret;
}

sub build_test_list
{
   # Collect tests filenames (all using SharedArrayBuffer but skipping those that use agent API).
   my @list = ();
   my $root="test262/built-ins/";
   my $content=`find $root -name "*.js" | xargs grep 'SharedArrayBuffer' | cut -d : -f 1 | sort -u`;
   foreach my $filename (split "\n", $content) {
      my $ret=`grep -c -E "agent|createRealm" $filename`;
      if ($ret == 0) {
         push @list, "\t\t\t"."\"/resources/$filename\"";
      }
   }
   # Print out as string.
   my @tests = ();
   push @tests, "var tests = [";
   push @tests, (join ",\n", @list);
   push @tests, "\t\t"."];";
   return join "\n", @tests;
}

sub generate_test262_runner
{
   my ($content, $tests) = @_;

   # Replace placeholders for test list.
   $content =~ s/###tests###/$tests/g;
   open (my $fh, '>', $OUTPUT);
   print $fh $content;
   print "Generated $OUTPUT. To run it:\n";
   print "./mach wpt-manifest-update\n";
   print "./mach wpt /atomics/test262-runner.html\n";
   close $fh;
}

# Read source file template.
my $content = readfile($TEMPLATE);

# Create list of tests to feed.
my $tests = build_test_list();

# Generate test262-runner.html file.
generate_test262_runner($content, $tests);

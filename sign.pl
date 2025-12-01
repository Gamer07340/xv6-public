#!/usr/bin/perl
# Sign the boot block with the boot signature 0x55AA

use strict;
use warnings;

die "Usage: $0 <bootblock-file>\n" unless @ARGV == 1;

my $file = $ARGV[0];

open(my $fh, '+<', $file) or die "Cannot open $file: $!\n";
binmode($fh);

# Read the entire file
my $buf;
my $n = sysread($fh, $buf, 1000);
die "sysread failed: $!\n" unless defined $n;

if($n > 510){
  print STDERR "ERROR: boot block too large: $n bytes (max 510)\n";
  exit 1;
}

print STDERR "Boot block is $n bytes (max 510)\n";

# Pad to 510 bytes
$buf .= "\0" x (510 - $n);

# Add boot signature
$buf .= "\x55\xAA";

# Verify we have exactly 512 bytes
die "Internal error: boot block is not 512 bytes\n" unless length($buf) == 512;

# Write back to file
seek($fh, 0, 0) or die "seek failed: $!\n";
print $fh $buf or die "write failed: $!\n";
truncate($fh, 512) or die "truncate failed: $!\n";
close($fh) or die "close failed: $!\n";

print STDERR "Boot block signed successfully (512 bytes with 0x55AA signature)\n";

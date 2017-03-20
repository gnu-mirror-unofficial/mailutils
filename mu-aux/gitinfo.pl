# This file is part of GNU Mailutils.
# Copyright (C) 2017 Free Software Foundation, Inc.
#
# GNU Mailutils is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License as
# published by the Free Software Foundation; either version 3, or (at
# your option) any later version.
#
# GNU Mailutils is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with GNU Mailutils.  If not, see <http://www.gnu.org/licenses/>.

use strict;
use Getopt::Long qw(:config gnu_getopt no_ignore_case);
use Pod::Usage;
use Pod::Man;
use Pod::Find qw(pod_where);

=head1 NAME

gitinfo.pl - build version tag for mailutils

=head1 SYNOPSIS

B<perl gitinfo.pl>
[B<-sv>]
[B<-C> I<DIR>]
[B<-H> I<FORMAT>]    
[B<-o> I<FILE>]
[B<--directory=>I<DIR>]
[B<--format=>I<FORMAT>]    
[B<--output=>I<FILE>]
[B<--stable>]
[B<--verbose>]

B<perl gitinfo.pl> B<-h> | B<--help> | B<--usage>

=head1 DESCRIPTION

Outputs Git information for the version of GNU mailutils in
the local source tree.  The information is printed according to
the supplied I<FORMAT>.  The format is an arbitrary string, containing
variable references in the form B<$I<VARIABLE>> or B<${I<VARIABLE>}>.
If the variable reference occurs within a quoted string, any occurrences
of double quotes and backslashes in the expansion will be escaped by
backslashes.
    
The following variables are defined:

=over 4

=item B<package>

Package name, obtained from the B<AC_INIT> line in F<configure.ac>.
    
=item B<version>

Package version, from the same source.    

=item B<refversion>

Reference version.  By default, it is the same as B<version> above.  If,
however, the B<-s> (B<--stable>) option is given, B<refversion> is set
to the most recent stable release.  The B<NEWS> file is analyzed in order
to find it.    

=item B<refdate>

Date when B<refversion> was released.  Normally this is meaningful only for
stable versions (in other words, when the B<-s> option is given).    
    
=item B<refcommit>

Hash of the commit corresponding to the B<refversion>.
    
=item B<n>

Number of commits between B<refcommit> and B<HEAD>.    

=item B<describe>

Result of the B<git describe> command.

=item B<dirty>

If the source tree has uncommitted modifications, this variable is set
to 1.  Otherwise, it is undefined.

=item B<commit_hash>

The hash of the topmost commit.    

=item B<commit_time>

Time of the topmost commit (UNIX timestamp).    

=item B<commit_subject>    

Subject of the topmost commit.
    
=back    

The construct

B<{?I<EXPR>??I<REPL-IF-TRUE>>[B<?|I<REPL-IF-FALSE>>]B<?}>

within a format string provides a rudimental control flow facility.  The
I<EXPR> is evaluated as a Perl expression in an environment when the variales
disucussed above are defined.  If the result is true (in Perl sense), the     
construct expands to I<REPL-IF-TRUE>.  Othervise, it expands to
I<REPL-IF-FALSE> or, if it is not defined, to the empty string.

Notice, that conditional expressions cannot be nested.
    
In addition to the format strings, the argument to the B<--format> option
can be any of the following predefined format names:

=over 4

=item B<h>

=item B<c>

Produces a set of B<C> defines.  It is equivalent to the following multi-line
format:

    #define MU_GIT_COMMIT_HASH "$commit_hash"
    #define MU_GIT_COMMIT_TIME "$commit_time"
    #define MU_GIT_COMMIT_SUBJECT "$commit_subject"
    {?$n>0??#define MU_GIT_COMMIT_DISTANCE $n
    ?}#define MU_GIT_DESCRIBE_STRING "$describe{?$dirty??-dirty?}"

=item B<all>

This is the default format.  It outputs all variables as B<I<NAME>="I<VALUE>">
pairs, each pair on a separate line.  Occurrences of B<"> and B<\> within
values are escaped by additional backslashes.
    
=back    
    
=head1 OPTIONS

=over 4

=item B<-C>, B<--directory=>I<DIR>

Use I<DIR> as the top-level source directory.  By default it is determined
automatically, which works for as long as B<gitinfo.pl> is run from a
subdirectory of the git-controlled working tree.

=item B<-H>, B<--format>=I<FORMAT>

Select output format.  See B<DESCRIPTION> for the details.    

=item B<-o>, B<--output=>I<FILE>

Output results to the I<FILE>, instead of the standard output.    
    
=item B<-s>, B<--stable>

Count versions from the most recent stable version.
    
=item B<-v>, B<--verbose>

Verbose output.

=item B<-h>

Display short help summary.

=item B<--help>

Display the manpage.

=item B<--usage>

Display command line usage summary.

=back

=cut

# find_commit(VERSION)
# Finds commit corresponding to the version number VERSION.
sub find_commit($) {
    my $v = quotemeta(shift);
    my $cmd = q{git log -S'^AC_INIT\(\[[^]]+\][[:space:]]*,[[:space:]]*\[}
	      . $v
	      . q{\]' --pickaxe-regex --reverse --pretty=format:%H -- configure.ac};
    open(my $fd, '-|', $cmd)
	or die "$cmd: $!";
    my $s = <$fd>;
    close $fd;
    chomp $s;
    return $s;
}

# find_count(VERSIN)
# Returns number of commits between VERSION and the current commit.
sub find_count($) {
    my $v = shift;
    my $cmd = "git rev-list --count $v..HEAD";
    open(my $fd, '-|', $cmd) or die "$cmd: $!";
    my $s = <$fd>;
    close $fd;
    chomp $s;
    return $s;
}

# recent_version(STABLE)
# Returns the most recent version described in the NEWS file.
# If STABLE is true, returns the most recent stable version, i.e.
# the one, for which release date is set.
sub recent_version($) {
    my ($stable) = @_;
    my $file = 'NEWS';
    open(my $fd, '<', $file) or die "can't open $file: $!";
    my ($version, $date);
    while (<$fd>) {
	chomp;
	if (/^(?:\*[[:space:]]+)?
	      [vV]ersion [[:space:]]+
	      ([[:digit:]](?:[.,][[:digit:]]+){1,2}(?:[[:digit:]._-])*)
	      (?:(?:.*)[[:punct:]][[:space:]]*
	      ([[:digit:]]{4}-[[:digit:]]{2}-[[:digit:]]{2}))?/x) {
	    next if ($stable && !defined($2));
	    ($version, $date) = ($1, $2);
	    last;
	}
    }
    close $fd;
    return ($version, $date);
}

# this_version()
# Returns a list (package, version).
sub this_version() {
    my $file = 'configure.ac';
    open(my $fd, '<', $file) or die "can't open $file: $!";
    while (<$fd>) {
	chomp;
	return ($1, $2) if (/^AC_INIT\(\[(.+?)\]\s*,\s*\[(.+?)\].*/);
    }
}

# git_describe($HASHREF)
# Define 'describe' and 'dirty' keys in $HASHREF
sub git_describe($) {
    my ($href) = @_;
    my $descr = `git describe`;
    chomp $descr;
    $href->{describe} = $descr;
    if (`git diff-index --name-only HEAD 2>/dev/null`) {
	$href->{dirty} = 1;
    }
    return $descr;
}

# last_commit_info($HASHREF)
# Populates %$HASHREF with entries describing the current commit.
sub last_commit_info {
    my ($hashref) = @_;
    my @names = qw(commit_hash commit_time commit_subject);
    open(my $fd,'-|',
	 "git log --max-count=1 --pretty=format:'%H%n%ai%n%s' HEAD")
	or die;
    while (<$fd>) {
	chomp;
	my $name = shift @names;
	last unless $name;
	$hashref->{$name} = $_;
    }
    close $fd;
}

# Convert POD markup to the usage message suitable for --help and --usage
# output.
sub pod_usage_msg() {
    my %args;
    open my $fd, '>', \my $msg;

    $args{-input} = pod_where({-inc => 1}, __PACKAGE__);
    pod2usage(-verbose => 99,
	      -sections => 'NAME',
	      -output => $fd,
	      -exitval => 'NOEXIT',
	      %args);
    my @a = split /\n/, $msg;
    $msg = $a[1];
    $msg =~ s/^\s+//;
    $msg =~ s/ - /: /;
    return $msg;
}

my %gitinfo;

sub eval_format {
    my ($format) = @_;
    my @res;
    while ($format) {
	if ($format =~ /^(?<pfx>.*?)"(?<sfx>.*)$/s) {
	    push @res, eval_format($+{pfx});
	    my $acc;
	    $format = $+{sfx};
	    my $s = $format;
	    while ($s) {
		if ($s =~ /^([^\\"]*?)\\"(.*)$/s) {
		    $acc .= $1 . '"';
		    $s = $2;
		} elsif ($s =~ /^(.*?)"(.*)$/s) {
		    $acc .= $1;
		    $format = $2;
		    last;
		} else {
		    $acc = undef;
		    last;
		}
	    }
	    if (defined($acc)) {
		my $x = eval_format($acc);
		$x =~ s/(["\\])/\\$1/g;
		push @res, '"', $x, '"';
	    }
	} elsif ($format =~ /^(?<pfx>.*?)
                             \{\?
                             (?<cond>.+?)
                             \?\?
                             (?<iftrue>.+?)
                             (?:\?\|(?<iffalse>.*?))?
                             \?\}(?<sfx>.*)$/sx) {
	    
	    my ($pfx, $cond, $iftrue, $iffalse, $sfx) =
		($+{pfx}, $+{cond}, $+{iftrue}, $+{iffalse}, $+{sfx});

	    use Safe;
	    my $s = new Safe;
	    while (my ($k,$v) = each %gitinfo) {
		${$s->varglob($k)} = $v;
	    }
	    my $x = $s->reval($cond);
	    push @res, eval_format($pfx);
	    if ($x) {
		push @res, eval_format($iftrue);
	    } else {
		push @res, eval_format($iffalse);
	    }
	    $format = $sfx;
	} elsif ($format =~ /^(?<pfx>.*?)
                        \$(?<ocb>{?)
                        (?<var>[a-zA-Z_][a-zA-Z_0-9]*)
                        (?<ccb>}?)
                        (?<sfx>.*)$/sx) {
	    my ($pfx, $ocb, $var, $ccb, $sfx) =
		($+{pfx}, $+{ocb}, $+{var}, $+{ccb}, $+{sfx});
	    if ("$ocb$ccb" =~ /^(?:{})?$/) {
		push @res, $pfx;
		my $val = $gitinfo{$var} if defined $gitinfo{$var};
		#$val =~ s/(["\\])/\\$1/g if $odq;
		push @res, $val;
		$format = $sfx;
	    } else {
		last;
	    }
	} else {
	    last;
	}
    }

    push @res, $format if $format;

    return join('', @res);
}

my $from_stable;
my $verbose;
my $output;
my $format = 'all';

my %fmtab = (
    c => <<'EOT'
#define MU_GIT_COMMIT_HASH "$commit_hash"
#define MU_GIT_COMMIT_TIME "$commit_time"
#define MU_GIT_COMMIT_SUBJECT "$commit_subject"
{?$n>0??#define MU_GIT_COMMIT_DISTANCE $n
?}#define MU_GIT_DESCRIBE_STRING "$describe{?$dirty??-dirty?}"
EOT
    ,
    'h' => 'c',
    'all' => sub {
	foreach my $name (sort keys %gitinfo) {
	    my $val = $gitinfo{$name};
	    $val =~ s/(["\\])/\\$1/g;
	    print "$name=\"$val\"\n";
	}
    }
);

my $dir;

GetOptions("help" => sub {
	      pod2usage(-exitstatus => 0, -verbose => 2);
	   },
	   "h" => sub {
	      pod2usage(-message => pod_usage_msg(),
			-exitstatus => 0);
	   },
	   "usage" => sub {
	      pod2usage(-exitstatus => 0, -verbose => 0);
	   },
	   "directory|C=s" => \$dir,
	   "stable|s" => \$from_stable,
	   "verbose|v" => \$verbose,
	   "format|H=s" => \$format,
	   "output|o=s" => \$output
    ) or exit(1);

if ($output) {
    open(STDOUT, '>', $output) or die "can't open $output: $!"
}

if ($dir) {
    chdir($dir) or die "can't change to $dir: $!";
} elsif (! -d '.git') {
    $dir = `git rev-parse --show-toplevel 2>/dev/null`;
    chomp $dir;
    chdir($dir) or die "can't change to $dir: $!";
}

if (-d '.git') {
    ($gitinfo{refversion}, my $date) = recent_version($from_stable);
    $gitinfo{refdate} = $date if defined $date;
    if ($verbose) {
	print STDERR "# counting from version $gitinfo{refversion}";
	print STDERR ", released on $gitinfo{refdate}"
	    if exists $gitinfo{refdate};
	print STDERR "\n";
    }

    $gitinfo{refcommit} = find_commit($gitinfo{refversion});
    print STDERR "# commit $gitinfo{refcommit}\n"
	if $verbose;
    my $n = find_count($gitinfo{refcommit});

    if ($n =~ /^[1-9][0-9]*$/) {
	$gitinfo{n} = $n;
    }
    last_commit_info(\%gitinfo);
    git_describe(\%gitinfo);
    ($gitinfo{package}, $gitinfo{version}) = this_version;
}

$format = $fmtab{$format} while exists $fmtab{$format};
if (ref($format) eq 'CODE') {
    &{$format}
} else {
    $format = "$format\n" unless $format =~ /\n$/s;
    print eval_format($format);
}


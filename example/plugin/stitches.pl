#!/usr/bin/perl

use warnings;
use strict;

use JSON;

my $json = JSON->new->utf8->pretty();

my @stitchisms = (
    "I LOVE SELLIN BLOW",
    "I'LL PUT THAT BRICK IN YO FACE",
    "IMMA PUT COCAINE IN YO ASS",
    "FUCK A JOB",
    "R.I.P. YOUNG JEEZY",
    "I AIN'T FUCKIN THAT BITCH IF THE PUSSY STANK",
    "I GET HIGH ON MY OWN SUPPLY"
);

sub irc_privmsg{
    my ($network, $target, $msg, $sender) = @_;

    my $privmsg = {
        type => "privmsg",
        network => $network,
        target => $target,
        msg => $msg
    };

    if($sender){
        $privmsg->{'msg'} = $sender.': '.$privmsg->{'msg'};
    }

    return $json->encode($privmsg);
}

sub match{
    my ($msg) = @_;

    if($msg =~ /blow/i){
        return $stitchisms[0];
    }
    elsif($msg =~ /brick/i){
        return $stitchisms[1];
    }
    elsif($msg =~ /ass/i){
        return $stitchisms[2];
    }
    elsif($msg =~ /pussy/i){
        return $stitchisms[5];
    }
    elsif($msg =~ /high/i){
        return $stitchisms[6];
    }
}

# Listen for messages from praetor and process them as they come in
my $obj;

while(<STDIN>){
    $obj = $json->incr_parse($_);
    if(defined($obj)){
        my $type = $obj->{'type'};

        if($type eq 'join'){
            print(irc_privmsg($obj->{'network'}, $obj->{'channel'}, $stitchisms[3]));
        }
        elsif($type eq 'part'){
            print(irc_privmsg($obj->{'network'}, $obj->{'channel'}, $stitchisms[4]));
        }
        elsif($type eq 'privmsg'){
            # If this was a PM, send to the user who sent us the PM
            my $target;
            if($obj->{'is_pm'} == JSON::true){
                $target = $obj->{'sender'};
            }
            else{
                $target = $obj->{'target'};
            }

            if($obj->{'is_hilight'}){
                print(irc_privmsg($obj->{'network'}, $target, match($obj->{'msg'}), $obj->{'sender'}));
            }
            else{
                print(irc_privmsg($obj->{'network'}, $target, match($obj->{'msg'})));
            }
        }
    }
    undef($obj);
}

if ! [ -e abc ]; then
    hg clone https://bitbucket.org/alanmi/abc
fi
cd abc 
hg update -r177bf2fa54b3


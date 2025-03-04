# Root specific bashrc
# Include the common bashrc
if [ -f /etc/skel/.bashrc ]; then
    . /etc/skel/.bashrc
fi

# Set a different prompt for root (red color)
PS1='${debian_chroot:+($debian_chroot)}\[\033[01;31m\]\u\[\033[01;33m\]@\[\033[01;36m\]\h \[\033[01;33m\]\w \[\033[01;35m\]\$ \[\033[00m\]'
# Root specific aliases
alias halt='shutdown -h now'
alias reboot='shutdown -r now'
alias netstat='netstat -tulanp'
alias ports='netstat -tulanp'

# Additional security aliases
alias rm='rm -i'
alias cp='cp -i'
alias mv='mv -i'
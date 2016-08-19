# -*- mode: ruby -*-
# vi: set ft=ruby :

VAGRANTFILE_API_VERSION = "2"
GOLANG_VERSION = '1.5.3'

Vagrant.configure(VAGRANTFILE_API_VERSION) do |config|
  config.vm.box = 'boxcutter/ubuntu1404'
  config.ssh.forward_agent = true

  %w{vmware_fusion vmware_workstation}.each do |provider|
    config.vm.provider(provider) do |v|
      v.vmx["memsize"] = "1024"
      v.vmx["numvcpus"] = "4"
    end
  end

  config.vm.synced_folder ".", "/vagrant", type: 'nfs'

  if File.exist?("#{ENV['HOME']}/code/linux")
    config.vm.synced_folder "#{ENV['HOME']}/code/linux", "/linux", type: 'nfs'
  end

  config.vm.network "forwarded_port",
                    guest: 8910,
                    host:  8910,
                    protocol: 'tcp',
                    host_ip: '127.0.0.1'


  config.vm.provision :shell, inline: <<EOF
set -ex
sudo apt-get update
sudo apt-get -y install software-properties-common
sudo apt-add-repository ppa:openjdk-r/ppa
sudo apt-get update
sudo apt-get -y install libgflags-dev libgit2-dev libjson0-dev \
  libboost-system-dev libboost-filesystem-dev libsparsehash-dev \
  build-essential git mercurial gdb openjdk-8-jdk unzip cmake
EOF

  config.vm.provision :shell, inline: <<EOF
set -ex
wget --no-verbose -O /usr/local/src/go#{GOLANG_VERSION}.tgz http://golang.org/dl/go#{GOLANG_VERSION}.linux-amd64.tar.gz
tar -C /usr/local -xzf /usr/local/src/go#{GOLANG_VERSION}.tgz
mkdir -p /gopath/src/github.com/livegrep/
ln -nsf /vagrant /gopath/src/github.com/livegrep/livegrep
chown -R vagrant:vagrant /gopath
cat >/etc/profile.d/golang.sh <<EOT
export PATH=$PATH:/usr/local/go/bin
export GOPATH=/gopath
EOT
EOF
end

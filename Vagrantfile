# -*- mode: ruby -*-
# vi: set ft=ruby :
 
Vagrant.configure("2") do |config|
  config.vm.box = "ubuntu/xenial64"
  config.vm.provision :shell, path: "bootstrap_vagrant.sh"
 
  config.vm.network "forwarded_port", guest: 8910, host: 8910
  config.vm.network "forwarded_port", guest: 8040, host: 8040
 
  config.vm.provider "virtualbox" do |vb|
    vb.memory = "4096"
    vb.cpus = 6
  end
end
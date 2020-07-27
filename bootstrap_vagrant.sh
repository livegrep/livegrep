sudo apt update
sudo apt install -y build-essential python

wget https://github.com/bazelbuild/bazelisk/releases/download/v1.5.0/bazelisk-linux-amd64
sudo install bazelisk-linux-amd64 /usr/local/bin/bazelisk
rm bazelisk-linux-amd64
echo "0.28.0" > /vagrant/.bazelversion

sudo apt-get install -y ruby-full
gem install sorbet

sudo apt install -y ucspi-tcp
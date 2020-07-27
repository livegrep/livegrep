#!/bin/bash
set -ex

WORK_DIR=$(mktemp -d --tmpdir=/tmp)

if [[ ! "$WORK_DIR" || ! -d "$WORK_DIR" ]]; then
	echo "Could not create temp dir"
	exit 1
fi

# deletes the temp directory, kills tcpserver if running
function cleanup() {
	rm -rf "$WORK_DIR"
	echo "Deleted temp working directory $WORK_DIR"
	kill -TERM "$child" 2>/dev/null
}

# register the cleanup function to be called on the EXIT signal
trap cleanup EXIT

# PAY_SERVER_REV=$(cd /pay/livegrep/stripe-internal/pay-server && git rev-parse HEAD)
# PAY_BUILD_ARTIFACT_BUCKET="stripe-builds-us-west-2"
# PAY_BUILD_ARTIFACT_KEY="stripe-internal-pay-server/$PAY_SERVER_REV/build.tar.gz"
# PAY_BUILD_ARTIFACT="s3://$PAY_BUILD_ARTIFACT_BUCKET/$PAY_BUILD_ARTIFACT_KEY"
# ARTIFACT_EXISTS=false

# # Wait for pay server build to complete/exist if it doesn't already
# for i in {1..10}; do
# 	ret=0
# 	aws s3api wait object-exists --bucket $PAY_BUILD_ARTIFACT_BUCKET --key=$PAY_BUILD_ARTIFACT_KEY || ret=$?
# 	if [ $ret -eq 255 ]; then
# 		# Timed out, try again
# 		continue
# 	elif [ $ret -eq 0 ]; then
# 		ARTIFACT_EXISTS=true
# 		break
# 	else
# 		echo "Failed whilst waiting for build artifact to exist"
# 		exit 1
# 	fi
# done

# if [ "$ARTIFACT_EXISTS" = false ]; then
# 	echo "Timed out waiting for build artifact to appear in S3"
# 	exit 1
# fi

# download and extract built pay-server with autogen run completed.
# aws s3 cp "s3://stripe-builds-us-west-2/stripe-internal-pay-server/$PAY_SERVER_REV/build.tar.gz" "/tmp/$PAY_SERVER_REV.tar.gz"

# tar -xvf "/tmp/$PAY_SERVER_REV.tar.gz" -C "$WORK_DIR"

# rm -f "/tmp/$PAY_SERVER_REV.tar.gz"

# tcpserver for some reason wants a single script to run (no flags), so create one.
cat >"$WORK_DIR"/lsp-server <<EOL
#!/bin/bash
exec $WORK_DIR/scripts/bin/typecheck --lsp
EOL

chmod +x "$WORK_DIR"/lsp-server

# run lsp-server in background, don't exec as we want to perform cleanup when this exits
tcpserver -c 5 -v 127.0.0.1 8040 "$WORK_DIR"/lsp-server >/pay/log/lsp-tcpserver.log 2>&1 &

# wait for lsp-server to exit, cleanup will run after.
child=$!
wait "$child"
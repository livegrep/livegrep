package reqid

import (
	"crypto/rand"
	"encoding/hex"
	"fmt"

	"golang.org/x/net/context"
)

type key int

const reqIDKey key = 0

type RequestID string

func New() RequestID {
	bytes := make([]byte, 16)
	if _, err := rand.Read(bytes); err != nil {
		panic(fmt.Sprintf("rand.Read: %v", err))
	}
	return RequestID(hex.EncodeToString(bytes))
}

func NewContext(ctx context.Context, reqID RequestID) context.Context {
	return context.WithValue(ctx, reqIDKey, reqID)
}

func FromContext(ctx context.Context) (RequestID, bool) {
	reqID, ok := ctx.Value(reqIDKey).(RequestID)
	return reqID, ok
}

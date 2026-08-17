FROM golang:1.26 AS build

WORKDIR /src/relay
COPY relay/go.mod ./
RUN go mod download
COPY relay/ ./
RUN CGO_ENABLED=0 go build -trimpath -ldflags="-s -w" -o /out/printdrop-relay .

FROM scratch

COPY --from=build /out/printdrop-relay /printdrop-relay
COPY web/ /web/

ENV PRINTDROP_RELAY_ADDR=:8080
ENV PRINTDROP_WEB_DIR=/web

USER 65532:65532
EXPOSE 8080
ENTRYPOINT ["/printdrop-relay"]

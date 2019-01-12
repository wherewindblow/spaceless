# Spaceless
Spaceless is file sharing program that can protect transferring data.

## Features
- Support user and group to sharing file.
- Support transferring from the point of interruption.
- Support secure transfer.

## Architeture
- client: provide basic feature using.
- resource server: manage user, group and storage node.
- storage node: provide storage to resource server.

## Dependency
- lights
- Poco 1.9.1 (lower that this version network may have not high performance)
- protobuf 3.6.1
- cryptopp 7.0.0
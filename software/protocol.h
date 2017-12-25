#ifndef PROTOCOL_H_
#define PROTOCOL_H_

typedef enum _ProtocolCommand {
  READ    = 1,
  WRITE   = 2,
  REVERSE = 3
} ProtocolCommand;

typedef enum _ProtocolReply {
  LED_NUMBER   = 1,
  SWITCH_COUNT = 2
} ProtocolReply;

#endif /* PROTOCOL_H_ */

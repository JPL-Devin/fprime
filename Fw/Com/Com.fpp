module Fw {

  type ComBuffer

  @ Port for passing communication packet buffers
  port Com(
            ref data: ComBuffer @< Buffer containing packet data
            context: U32 @< Call context value; meaning chosen by user
          )

  @ Port for passing communication packet buffers along with their APID (packet type)
  port ComBufferWithApid(
            ref data: ComBuffer @< Buffer containing packet data
            apid: ComCfg.Apid @< APID (packet type) of the data
          )

}

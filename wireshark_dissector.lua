-- Define MicroTCP protocol
microtcp = Proto("microtcp", "MicroTCP Protocol")

-- Define protocol fields
microtcp.fields.seq_number   = ProtoField.uint32("microtcp.seq_number", "Sequence Number", base.DEC)
microtcp.fields.ack_number   = ProtoField.uint32("microtcp.ack_number", "ACK Number", base.DEC)
microtcp.fields.control      = ProtoField.uint16("microtcp.control", "Control Flags", base.HEX)
microtcp.fields.window       = ProtoField.uint32("microtcp.window", "Window Size", base.DEC)
microtcp.fields.data_len     = ProtoField.uint32("microtcp.data_len", "Payload Length", base.DEC)
microtcp.fields.future_use0  = ProtoField.uint32("microtcp.future_use0", "Reserved 0", base.HEX)
microtcp.fields.future_use1  = ProtoField.uint32("microtcp.future_use1", "Reserved 1", base.HEX)
microtcp.fields.future_use2  = ProtoField.uint32("microtcp.future_use2", "Reserved 2", base.HEX)
microtcp.fields.checksum     = ProtoField.uint32("microtcp.checksum", "Checksum (CRC-32)", base.HEX)

-- Dissector function
function microtcp.dissector(buffer, pinfo, tree)
    -- Check if the buffer is long enough for a MicroTCP header
    if buffer:len() < 34 then return end

    -- Set protocol name in Wireshark
    pinfo.cols.protocol = "MicroTCP"

    -- Create protocol subtree
    local subtree = tree:add(microtcp, buffer(), "MicroTCP Header")

    -- Extract fields from the buffer
    subtree:add(microtcp.fields.seq_number, buffer(0,4))
    subtree:add(microtcp.fields.ack_number, buffer(4,4))
    subtree:add(microtcp.fields.control, buffer(8,2))
    subtree:add(microtcp.fields.window, buffer(10,4))
    subtree:add(microtcp.fields.data_len, buffer(14,4))
    subtree:add(microtcp.fields.future_use0, buffer(18,4))
    subtree:add(microtcp.fields.future_use1, buffer(22,4))
    subtree:add(microtcp.fields.future_use2, buffer(26,4))
    subtree:add(microtcp.fields.checksum, buffer(30,4))

    -- Optional: Display UDP Payload as MicroTCP Data
    local data_len = buffer(14,4):uint()
    if buffer:len() > 34 then
        subtree:add(buffer(34, data_len), "MicroTCP Data")
    end
end

-- Register dissector for a specific UDP port
udp_table = DissectorTable.get("udp.port")
udp_table:add(50505, microtcp)  -- Change this to your actual MicroTCP port

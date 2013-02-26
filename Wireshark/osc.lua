-- based on http://gitorious.org/~jabbott42/wireshark-osc/jabbott42s-master
-- add this to "C:\program files\wireshark\plugins\<wireshark version>\" or simular directory on other OS
-- GNU General Public License version 3 (GPLv3)
--
do
    -- make a convertor for unsigned w-bit integers to corresponding two complement signed integers
    function makeU2I(w)
        local full = 2^(w) -- max unsigned + 1
        local half = 2^(w-1) -- max signed
        return function(n) 
            if n < half then
                return n
            else
                return n - full
            end 
        end
    end

    u2i32 = makeU2I(32)
    u2i64 = makeU2I(64)

    function ntp2float(sec,frac)
        return sec + frac/(2^32)
    end

    -- OSC

    local info
    osc = Proto("osc", "OSC", "Open Sound Control")
    function osc.dissector(buffer, pinfo, tree)
        function chunk(offs, len, tree)
            message("chunk(" .. offs .. ", " .. len .. ")")

            local advance = function(n)
                offs = offs + n
                len = len - n
                message("advance offs=" .. offs .. ", len=" .. len .. "")
            end

            local getString = function()
                local res = {}
                local start = offs
                local n = 0
                local stringTermMissing=false
                while (buffer(offs+n,1):uint() ~= 0) do
                    --message("n=" .. n .. ", len=" .. len .. "")
                    n=n+1
                    if(n>=len) then
                        stringTermMissing = true
                        break
                    end
                end
                --message("stringlen=" .. n)
                res.str = buffer(offs,n):string()
                if not stringTermMissing then
                    n = n + 1 
                end
                n = n + (4-(n%4))%4
                --message("advance n=" .. n .. ", len=" .. len .. "")
                advance(n)
                --message("advanced")
                res.range = buffer(start,n)
                --message("done")
                return res
            end

            local getTimeTag = function()
                local res = {}
                res.range = buffer(offs,8)
                local sec = buffer(offs,4):uint()
                local frac = buffer(offs+4,4):uint()
                advance(8)
                if (sec==0 and frac==0) then
                    res.str = "NTP{Bad time}"
                elseif (sec==0 and frac == 1) then
                    res.str = "OSC{immediate}"
                else
                    res.str = "NTP{" .. sec .. ":" .. frac .. "} / " .. ntp2float(sec,frac) .. " sec"
                end
                return res
            end

            local msg = function(tree)
                local full_range = buffer(offs,len)
                local path = getString()
                local msg_tree = tree:add(full_range,"OSC Message: '" .. path.str .. "'")
                message("OSC Message: " .. path.str)
                info=path.str
                msg_tree:add(path.range,"OSC Address Pattern: " .. path.str)
                message("OSC Address Pattern: " .. path.str)
                if len>0 and buffer(offs,1):string() == "," then
                    local types = getString(offs, len)
                    local t = string.sub(types.str,2)
                    msg_tree:add(types.range,"OSC Type string: " .. t)
                    message("OSC Type Tag String: " .. t .. " types=" .. types.str)
                    i = 0
                    info = info .. "," .. t
                    for t in string.gmatch(t,'.') do
                        i = i + 1
                        if t=='i' then
                            msg_tree:add(buffer(offs,4),"int32: " .. u2i32(buffer(offs,4):uint()))
                            info = info .. " " .. u2i32(buffer(offs,4):uint())
                            advance(4)
                        elseif t=='h' then
                            msg_tree:add(buffer(offs,8),"int64: " .. u2i64(buffer(offs,8):uint()))
                            info = info .. " " .. u2i64(buffer(offs,8):uint())
                            advance(8)
                        elseif t=='f' then
                            msg_tree:add(buffer(offs,4),"float: " .. buffer(offs,4):float())
                            info = info .. " " .. buffer(offs,4):float()
                            advance(4)
                        elseif t=='d' then
                            msg_tree:add(buffer(offs,8),"double: " .. buffer(offs,8):float())
                            info = info .. " " .. buffer(offs,8):double()
                            advance(8)
                        elseif t=='s' or t=='S' then
                            local string = "string"
                            if t=='S' then string = "symbol" end
                            local s = getString()
                            info = info .. " " .. s.str
                            msg_tree:add(s.range, string .. ": " .. s.str)
                        elseif t=='T' or t=='F' then
                            info = info .. " " .. t
                            msg_tree:add(types.range(i,1),"bool: " .. t)
                        elseif t=='t' then
                            local tt = getTimeTag()
                            info = info .. " " .. tt.str
                            msg_tree:add(tt.range, "timetag: " .. tt.str)
                        elseif t=='b' then
                            message("blob offset=" .. offs .. " len=" .. len)
                            --msg_tree:add(buffer(offs,len),"blob: " .. buffer(offs,4):bytes())
                            msg_tree:add(buffer(offs,len),"blob: ")
                            if path.str == "meters/0" then
                                local length = u2i32(buffer(offs,4):uint())
                                info = info .. " " .. u2i32(buffer(offs,4):uint())
                                msg_tree:add(buffer(offs,4),"length: " .. u2i32(buffer(offs,4):uint()))
                                length = (length - 4) / 4
                                advance(4)
                                msg_tree:add(buffer(offs,4),"unknown: " .. u2i32(buffer(offs,4):uint()))
                                advance(4)
                                local i=1
                                local j=1
                                while i<=32 and i<=length do
                                    msg_tree:add(buffer(offs,4),"channel input[" .. j .. "]: " .. buffer(offs,4):float())
                                    advance(4)
                                    i=i+1
                                    j=j+1
                                end
                                j=1
                                while i<=40 and i<=length do
                                    msg_tree:add(buffer(offs,4),"aux return[" .. j .. "]: " .. buffer(offs,4):float())
                                    advance(4)
                                    i=i+1
                                    j=j+1
                                end
                                j=1
                                while i<=48 and i<=length do
                                    msg_tree:add(buffer(offs,4),"st fx return[" .. j .. "][1]: " .. buffer(offs,4):float())
                                    advance(4)
                                    msg_tree:add(buffer(offs,4),"st fx return[" .. j .. "][2]: " .. buffer(offs,4):float())
                                    advance(4)
                                    i=i+2
                                    j=j+1
                                end
                                j=1
                                while i<=64 and i<=length do
                                    msg_tree:add(buffer(offs,4),"bus master[" .. j .. "]: " .. buffer(offs,4):float())
                                    advance(4)
                                    i=i+1
                                    j=j+1
                                end
                                j=1
                                while i<=70 and i<=length do
                                    msg_tree:add(buffer(offs,4),"matrix[" .. j .. "]: " .. buffer(offs,4):float())
                                    advance(4)
                                    i=i+1
                                    j=j+1
                                end
                            elseif path.str == "meters/1" then
                                local length = u2i32(buffer(offs,4):uint())
                                info = info .. " " .. u2i32(buffer(offs,4):uint())
                                msg_tree:add(buffer(offs,4),"length: " .. u2i32(buffer(offs,4):uint()))
                                length = (length - 4) / 4
                                advance(4)
                                msg_tree:add(buffer(offs,4),"unknown: " .. u2i32(buffer(offs,4):uint()))
                                advance(4)
                                local i=1
                                local j=1
                                while i<=32 and i<=length do
                                    msg_tree:add(buffer(offs,4),"channel input[" .. j .. "]: " .. buffer(offs,4):float())
                                    advance(4)
                                    i=i+1
                                    j=j+1
                                end
                                j=1
                                while i<=64 and i<=length do
                                    msg_tree:add(buffer(offs,4),"gate chain reduction[" .. j .. "]: " .. buffer(offs,4):float())
                                    advance(4)
                                    i=i+1
                                    j=j+1
                                end
                                j=1
                                while i<=96 and i<=length do
                                    msg_tree:add(buffer(offs,4),"dynamic gain reduction[" .. j .. "]: " .. buffer(offs,4):float())
                                    advance(4)
                                    i=i+1
                                    j=j+1
                                end
                            elseif path.str == "meters/2" then
                                local length = u2i32(buffer(offs,4):uint())
                                info = info .. " " .. u2i32(buffer(offs,4):uint())
                                msg_tree:add(buffer(offs,4),"length: " .. u2i32(buffer(offs,4):uint()))
                                length = (length - 4) / 4
                                advance(4)
                                msg_tree:add(buffer(offs,4),"unknown: " .. u2i32(buffer(offs,4):uint()))
                                advance(4)
                                local i=1
                                local j=1
                                while i<=16 and i<=length do
                                    msg_tree:add(buffer(offs,4),"bus master[" .. j .. "]: " .. buffer(offs,4):float())
                                    advance(4)
                                    i=i+1
                                    j=j+1
                                end
                                j=1
                                while i<=22 and i<=length do
                                    msg_tree:add(buffer(offs,4),"matrix[" .. j .. "]: " .. buffer(offs,4):float())
                                    advance(4)
                                    i=i+1
                                    j=j+1
                                end
                                j=1
                                while i<=24 and i<=length do
                                    msg_tree:add(buffer(offs,4),"main[" .. j .. "]: " .. buffer(offs,4):float())
                                    advance(4)
                                    i=i+1
                                    j=j+1
                                end
                                j=1
                                while i<=25 and i<=length do
                                    msg_tree:add(buffer(offs,4),"mono m/c[" .. j .. "]: " .. buffer(offs,4):float())
                                    advance(4)
                                    i=i+1
                                    j=j+1
                                end
                                j=1
                                while i<=41 and i<=length do
                                    msg_tree:add(buffer(offs,4),"dynamic gain reduction[" .. j .. "]: " .. buffer(offs,4):float())
                                    advance(4)
                                    i=i+1
                                    j=j+1
                                end
                                j=1
                                while i<=47 and i<=length do
                                    msg_tree:add(buffer(offs,4),"matrix dynamic gain reduction[" .. j .. "]: " .. buffer(offs,4):float())
                                    advance(4)
                                    i=i+1
                                    j=j+1
                                end
                                j=1
                                while i<=48 and i<=length do
                                    msg_tree:add(buffer(offs,4),"main lr dynamic gain reduction[" .. j .. "]: " .. buffer(offs,4):float())
                                    advance(4)
                                    i=i+1
                                    j=j+1
                                end
                                j=1
                                while i<=49 and i<=length do
                                    msg_tree:add(buffer(offs,4),"momo m/c dynamic gain reduction[" .. j .. "]: " .. buffer(offs,4):float())
                                    advance(4)
                                    i=i+1
                                    j=j+1
                                end
                            elseif path.str == "meters/3" then
                                local length = u2i32(buffer(offs,4):uint())
                                info = info .. " " .. u2i32(buffer(offs,4):uint())
                                msg_tree:add(buffer(offs,4),"length: " .. u2i32(buffer(offs,4):uint()))
                                length = (length - 4) / 4
                                advance(4)
                                msg_tree:add(buffer(offs,4),"unknown: " .. u2i32(buffer(offs,4):uint()))
                                advance(4)
                                local i=1
                                local j=1
                                while i<=6 and i<=length do
                                    msg_tree:add(buffer(offs,4),"aux send[" .. j .. "]: " .. buffer(offs,4):float())
                                    advance(4)
                                    i=i+1
                                    j=j+1
                                end
                                j=1
                                while i<=14 and i<=length do
                                    msg_tree:add(buffer(offs,4),"aux return[" .. j .. "]: " .. buffer(offs,4):float())
                                    advance(4)
                                    i=i+1
                                    j=j+1
                                end
                                j=1
                                while i<=22 and i<=length do
                                    msg_tree:add(buffer(offs,4),"st fx return[" .. j .. "][1]: " .. buffer(offs,4):float())
                                    advance(4)
                                    msg_tree:add(buffer(offs,4),"st fx return[" .. j .. "][2]: " .. buffer(offs,4):float())
                                    advance(4)
                                    i=i+2
                                    j=j+1
                                end
                            elseif path.str == "meters/4" then
                                local length = u2i32(buffer(offs,4):uint())
                                info = info .. " " .. u2i32(buffer(offs,4):uint())
                                msg_tree:add(buffer(offs,4),"length: " .. u2i32(buffer(offs,4):uint()))
                                length = (length - 4) / 4
                                advance(4)
                                msg_tree:add(buffer(offs,4),"unknown: " .. u2i32(buffer(offs,4):uint()))
                                advance(4)
                                local i=1
                                local j=1
                                while i<=32 and i<=length do
                                    msg_tree:add(buffer(offs,4),"channel input[" .. j .. "]: " .. buffer(offs,4):float())
                                    advance(4)
                                    i=i+1
                                    j=j+1
                                end
                                j=1
                                while i<=40 and i<=length do
                                    msg_tree:add(buffer(offs,4),"aux return[" .. j .. "]: " .. buffer(offs,4):float())
                                    advance(4)
                                    i=i+1
                                    j=j+1
                                end
                                j=1
                                while i<=56 and i<=length do
                                    msg_tree:add(buffer(offs,4),"output[" .. j .. "]: " .. buffer(offs,4):float())
                                    advance(4)
                                    i=i+1
                                    j=j+1
                                end
                                j=1
                                while i<=72 and i<=length do
                                    msg_tree:add(buffer(offs,4),"p16 ultranet output[" .. j .. "]: " .. buffer(offs,4):float())
                                    advance(4)
                                    i=i+1
                                    j=j+1
                                end
                                j=1
                                while i<=78 and i<=length do
                                    msg_tree:add(buffer(offs,4),"aux send[" .. j .. "]: " .. buffer(offs,4):float())
                                    advance(4)
                                    i=i+1
                                    j=j+1
                                end
                                j=1
                                while i<=80 and i<=length do
                                    msg_tree:add(buffer(offs,4),"digital aes/ebu out[" .. j .. "]: " .. buffer(offs,4):float())
                                    advance(4)
                                    i=i+1
                                    j=j+1
                                end
                                j=1
                                while i<=82 and i<=length do
                                    msg_tree:add(buffer(offs,4),"monitor out[" .. j .. "]: " .. buffer(offs,4):float())
                                    advance(4)
                                    i=i+1
                                    j=j+1
                                end
                            elseif path.str == "meters/5" then
                                msg_tree:add(buffer(offs,4),"length: " .. u2i32(buffer(offs,4):uint()))
                                advance(4)
                                msg_tree:add(buffer(offs,4),"chn_meter_id: " .. u2i32(buffer(offs,4):uint()))
                                advance(4)
                                msg_tree:add(buffer(offs,4),"grp_meter_id: " .. u2i32(buffer(offs,4):uint()))
                                advance(4)
                            elseif path.str == "meters/6" then
                                msg_tree:add(buffer(offs,4),"length: " .. u2i32(buffer(offs,4):uint()))
                                advance(4)
                                msg_tree:add(buffer(offs,4),"chn_meter_id: " .. u2i32(buffer(offs,4):uint()))
                                advance(4)
                            elseif path.str == "meters/7" then
                                local length = u2i32(buffer(offs,4):uint())
                                info = info .. " " .. u2i32(buffer(offs,4):uint())
                                msg_tree:add(buffer(offs,4),"length: " .. u2i32(buffer(offs,4):uint()))
                                length = (length - 4) / 4
                                advance(4)
                                msg_tree:add(buffer(offs,4),"unknown: " .. u2i32(buffer(offs,4):uint()))
                                advance(4)
                                local i=1
                                local j=1
                                while i<=16 and i<=length do
                                    msg_tree:add(buffer(offs,4),"bus send meter[" .. j .. "]: " .. buffer(offs,4):float())
                                    advance(4)
                                    i=i+1
                                    j=j+1
                                end
                            elseif path.str == "meters/8" then
                                local length = u2i32(buffer(offs,4):uint())
                                info = info .. " " .. u2i32(buffer(offs,4):uint())
                                msg_tree:add(buffer(offs,4),"length: " .. u2i32(buffer(offs,4):uint()))
                                length = (length - 4) / 4
                                advance(4)
                                msg_tree:add(buffer(offs,4),"unknown: " .. u2i32(buffer(offs,4):uint()))
                                advance(4)
                                local i=1
                                local j=1
                                while i<=6 and i<=length do
                                    msg_tree:add(buffer(offs,4),"matrix send meter[" .. j .. "]: " .. buffer(offs,4):float())
                                    advance(4)
                                    i=i+1
                                    j=j+1
                                end
                            elseif path.str == "meters/9" then
                                local length = u2i32(buffer(offs,4):uint())
                                info = info .. " " .. u2i32(buffer(offs,4):uint())
                                msg_tree:add(buffer(offs,4),"length: " .. u2i32(buffer(offs,4):uint()))
                                length = (length - 4) / 4
                                advance(4)
                                msg_tree:add(buffer(offs,4),"unknown: " .. u2i32(buffer(offs,4):uint()))
                                advance(4)
                                local i=1
                                local j=1
                                while i<=32 and i<=length do
                                    msg_tree:add(buffer(offs,4),"fx   send[" .. j .. "][1]: " .. buffer(offs,4):float())
                                    advance(4)
                                    msg_tree:add(buffer(offs,4),"fx   send[" .. j .. "][2]: " .. buffer(offs,4):float())
                                    advance(4)
                                    msg_tree:add(buffer(offs,4),"fx return[" .. j .. "][1]: " .. buffer(offs,4):float())
                                    advance(4)
                                    msg_tree:add(buffer(offs,4),"fx return[" .. j .. "][2]: " .. buffer(offs,4):float())
                                    advance(4)
                                    i=i+4
                                    j=j+1
                                end
                            end
                            advance(len)
                        else 
                            break
                        end
                    end
                else
                    -- can't parse
                    msg_tree:add(buffer(offs,len), "Opaque OSC data")
                end
            end

            local bundle = function(tree)
                tree = tree:add(buffer(offs,len),"OSC Bundle")
                tree:add(buffer(offs,8),"Bundle marker")
                advance(8)
                local tt = getTimeTag()
                tree:add(tt.range,"Bundle time tag: " .. tt.str)
                while len > 0 do
                    local elem_len = buffer(offs,4):uint()
                    message("Bundle: len=" .. len .. " elem_len=" .. elem_len)

                    local elem_tree = tree:add(buffer(offs,elem_len+4), "Bundle element")
                    elem_tree:add(buffer(offs,4), "Element size: " .. elem_len)

                    chunk(offs+4, elem_len, elem_tree)
                    message("here offs: " .. offs .. " len: " .. len)
                    advance(elem_len + 4)
                end
            end
            --if buffer(offs,1):string() == '/' then
                msg(tree)
            --else --bundle is coded different
            --    bundle(tree)
            --end
        end
        pinfo.cols.protocol = "OSC"
        pinfo.cols.info = (info or "NIL")
        chunk(0, buffer:len(), tree)
    end

    -- register protocols
    udp_table = DissectorTable.get("udp.port")
    udp_table:add(10023,osc)
end
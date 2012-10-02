require 'cinch'

def set_msg(m)
  f = IO.popen("socat -T 1 STDIO UDP:mpd:1337", mode="w+")
  f.write(m + " ")
  return f.read()
end 


bot = Cinch::Bot.new do
  configure do |c|
    c.nick = "FallblattAnzeige"
    c.server = "irc.us.hackint.eu"
    c.channels = ["#chaos-darmstadt"]
#    c.channels = ["#fallblatt"]
  end

  on :private do |m|
     m.reply set_msg(m.params[1])
  end

  on :message, /fallblatt/i do |m|
     msg = m.params[1]
     msg = /Fallblatt[a-zA-Z]*:[ ]?(.*)/i.match(msg)[1] rescue msg

     m.reply set_msg(msg)
#     f = IO.popen("socat -T 1 STDIO UDP:mpd:1337", mode="w+")
#     f.write(m.params[1] + " ")
#     m.reply(f.read())
  end
end

bot.start



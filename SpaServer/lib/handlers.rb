require 'set'
require 'em-serialport'
require_relative 'protocol'

class SpaHandler < EM::Connection
  include EM::Protocols::LineText2

  attr_accessor :svr

  def initialize pfx
    @pfx = pfx
    @svr = nil
    puts "#{@pfx} #{self.class.to_s}"
  end

end

class SocketHandler < SpaHandler
  def initialize
    super '%% '
  end

  def unbind
    svr.connections.delete self
  end

  def receive_line line
    puts "#{@pfx} #{line}"
    @svr.receive_from_client line
  end
end

class SerialHandler < SpaHandler
  def initialize
    super '>> '
  end

  def receive_line (line)
    puts "#{@pfx} #{line}"
    if line == "syn"
      ts = "ti #{Time.now.to_i.to_s}\n"
      send_data ts
    else
      svr.send_to_clients line unless @svr.nil?
    end
  end
end

class KeyboardHandler < SpaHandler

  def initialize
    @quit_commands = ['exit', 'x', 'quit', 'q'].to_set
    super '## '
  end

  def receive_line line
    exit 0 if @quit_commands.member? line
    @svr.serial.send_data "#{line}\n"
  end
end

class SpaServer

  attr_accessor :connections
  attr_accessor :serial


  def initialize sp
    @sp = sp
    @connections = []
  end

  def send_to_clients data
    w = SpaProtocol.serial_to_proto_wire data
    return if w.nil?

    @connections.each do |c|
      c.send_data w
    end
  end

  def receive_from_client data
    puts data
    @serial.send_data "#{data}\n"
  end

  def start
    @serial = EM::open_serial(@sp, 9600, 8, 1, 0, SerialHandler) do |con|
      con.svr = self
    end

    @keyboard = EM::open_keyboard(KeyboardHandler) do |con|
      con.svr = self
    end

    @socket = EM::start_server('0.0.0.0', 8782, SocketHandler) do |con|
      con.svr = self
      connections << con
      @serial.send_data "st\nrly\ntmp\n"
    end
  end

  def stop
    EM::stop_server(@socket)
    unless wait_for_connections_and_stop
      EM::add_periodic_timer(1) { wait_for_connections_and_stop }
    end
  end

  def wait_for_connections_and_stop
    if @connections.empty?
      EM::stop
      true
    else
      puts "Waiting for #{@connections.size} connection(s) to finish ..."
      false
    end
  end

end

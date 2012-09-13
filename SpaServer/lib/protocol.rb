require_relative 'spa.pb.rb'

module SpaProtocol

  class SerialMessage

    attr_accessor :tokens

    def initialize str
      @str = str
      @tokens = @str.split(/ /)
    end

    def p_s
      @tokens.shift
    end

    def p_b
      !@tokens.shift.to_i.zero?
    end

    def p_f
      @tokens.shift.to_f
    end

    def p_i
      @tokens.shift.to_i
    end

    def to_proto
      case @tokens.shift
        when 'rly'
          Relays.new          :safety           => p_b,
                              :pump             => p_b,
                              :heat             => p_b,
                              :aux              => p_b
        when 'tmp'
          Temperature.new     :temperature      => p_f,
                              :set_point        => p_f,
                              :derivative       => p_f,
                              :trigger_state    => p_i
        when 'st'
          States.new          :mode             => p_i,
                              :pump             => p_i,
                              :aux              => p_i,
                              :status           => p_i
        when 'stm'
          ScheduleTimers.new  :period           => p_i,
                              :min_duty         => p_i,
                              :max_duty         => p_i,
                              :cycle_start      => p_i,
                              :manual_duration  => p_i,
                              :cycle_elapsed    => p_i,
                              :duty_elapsed     => p_i
        else
          nil
      end
    end

  end

  def self.serial_to_proto_wire str
    p = serial_to_proto str
    return if p.nil?

    w = wrap_proto p
    return if w.nil?

    w.to_s
  end

  def self.serial_to_proto str
    SerialMessage.new(str).to_proto
  end

  def self.wrap_proto payload
    SpaMessage.new :class_name    => payload.class.name,
                   :message       => payload.to_s
  end
end
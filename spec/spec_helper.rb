require 'rspec'
require 'redis'
require 'curb'

def header_str_to_map (header_str)
  http_response, *http_headers = header_str.split(/[\r\n]+/).map(&:strip)
  Hash[http_headers.flat_map{ |s| s.scan(/^(\S+): (.+)/) }]
end

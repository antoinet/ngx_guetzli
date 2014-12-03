require 'rspec'
require 'redis'
require 'curb'

def parse_header_str (header_str)
   http_response, *http_headers = header_str.split(/[\r\n]+/).map(&:strip)
   http_headers.flat_map{ |s| s.scan(/^(\S+): (.+)/) }
end

def get_cookies (header_str)
  lines = header_str.split(/[\r\n]+/).map(&:strip)
  cookies = lines.select{ |h| h =~ /^Set-Cookie:/i } 
  cookies.map{ |s| s.slice(/^Set-Cookie: (.+)/, 1) }
end

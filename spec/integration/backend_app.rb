require 'sinatra'

File.open('sinatra.pid', 'w') {|f| f.write Process.pid}

get '/' do
    status 200
    headers "Set-Cookie" => "SINATRA=test;path=/;secure;HttpOnly"
    body {
      "cookies: " + request.cookies.map{|key, val| key + "=>" + val}.join(",")
    }
end


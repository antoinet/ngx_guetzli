require 'sinatra'

get '/' do
    status 200
    headers "Set-Cookie" => "SINATRA=test;path=/;secure;HttpOnly"
    body "hello, world"
end


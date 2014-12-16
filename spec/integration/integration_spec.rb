require 'spec_helper'

describe "Integration specs" do

  SESSIONID = "sessionid"

  before do
    @redis = Redis.new
    @redis.flushdb
  end

  after do
    @redis.flushdb
  end

  describe "Bootstrap" do
    it "Webserver is running" do
      expect(Curl.get("http://127.0.0.1:8888").response_code).to eq(200)
    end
  end


  describe "Use cases" do
    it "Sets a sessionid when no session cookie is present." do
      http = Curl.get("http://127.0.0.1:8888") do |http|
        http.headers['Cookie'] = ""
      end

      cookies = get_cookies(http.header_str)
      expect(cookies).to include (a_string_matching(/^#{SESSIONID}=/))
    end

    it "Sets a new sessionid when session cookie is invalid." do
      http = Curl.get("http://127.0.0.1:8888") do |http|
        http.headers['Cookie'] = "#{SESSIONID}=invalid"
      end

      cookies = get_cookies(http.header_str)
      expect(cookies).to include (a_string_matching(/^#{SESSIONID}=/))
    end

    it "Does not set the sessionid when the session cookie is valid." do
      @redis.set(SESSIONID, "00000000000000000000000000000000")
      http = Curl.get("http://127.0.0.1:8888") do |http|
        http.headers['Cookie'] = "#{SESSIONID}=00000000000000000000000000000000"
      end

      cookies = get_cookies(http.header_str)
      expect(cookies).not_to include (a_string_matching(/^#{SESSIONID}=/))
    end

    it "Does not set any other cookie than the sessionid." do
      http = Curl.get("http://127.0.0.1:8888")

      cookies = get_cookies(http.header_str)
      cookies.select!{ |c| c !~ /^#{SESSIONID}/ }
      expect(cookies).to be_empty
    end

    it "Sets HttpOnly and secure flags to the session cookie." do
      http = Curl.get("http://127.0.0.1:8888")

      cookies = get_cookies(http.header_str)
      session_cookie = cookies.find{ |e| e =~ /^#{SESSIONID}/}
      expect(session_cookie).to end_with(";secure;HttpOnly")
    end

    it "Doesn't forward cookies from the browser to the backend application." do
      http = Curl.get("http://127.0.0.1:8888") do |http|
        http.headers['Cookie'] = "foo=bar"
      end

      expect(http.body).not_to include("foo")
    end
  end
end

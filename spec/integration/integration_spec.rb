require 'spec_helper'

describe "Integration specs" do

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
    it "Returns no values from redis when the guetzli session cookie is not present." do
      http = Curl.get("http://127.0.0.1:8888") do |http|
        http.headers['Cookie'] = ""
      end

      expect(http.headers['X-Guetzli-Debug']).to be_nil
    end

    it "Ignores invalid session cookies." do
      @redis.set("test", "success")
      http = Curl.get("http://127.0.0.1:8888") do |http|
        http.headers['Cookie'] = "guetzli_sess=fail"
      end

      headers = header_str_to_map(http.header_str)

      expect(headers['X-Debug-Guetzli']).to be_nil
    end

    it "Resolves values from redis when the guetzli session cookie is present." do
      @redis.set("test", "success")
      http = Curl.get("http://127.0.0.1:8888") do |http|
        http.headers['Cookie'] = "guetzli_sess=test"
      end

      headers = header_str_to_map(http.header_str)

      expect(headers['X-Guetzli-Debug']).to eq('success')
    end    
  end
end

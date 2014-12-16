require 'rake'
require 'rspec/core/rake_task'

RSpec::Core::RakeTask.new(:integration) do |t|
  t.pattern = "spec/**/*_spec.rb"
end

namespace :sinatra do
  desc "Starts sinatra"
  task :start do
    `nohup ruby spec/integration/backend_app.rb >> sinatra.log 2>&1 &`
  end

  desc "Stops sinatra"
  task :stop do
    `kill \`cat sinatra.pid\` && rm sinatra.pid`
  end
end

namespace :nginx do
  desc "Starts NGINX"
  task :start do
    `build/nginx/sbin/nginx`
    sleep 1
  end

  desc "Stops NGINX"
  task :stop do
    `build/nginx/sbin/nginx -s stop`
  end

  desc "Stops and starts NGINX"
  task :restart => ["nginx:stop", "nginx:start"]

  desc "Recompiles NGINX"
  task :compile do
    sh "script/compile"
  end
end

desc "Bootstraps the local development environment"
task :bootstrap do
  unless Dir.exists?("build") and Dir.exists?("vendor")
    sh "script/bootstrap"
  end
end

desc "Cleans the local development environment"
task :cleanup do
  sh "script/bootstrap clean"
end

desc "Run the integration tests"
task :default => [:bootstrap, "sinatra:start", "nginx:start", :integration, "nginx:stop", "sinatra:stop"]


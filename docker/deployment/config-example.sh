export UWSGI_PROCESSES=2
export UWSGI_THREADS=2

export UWSGI_UID=100
export UWSGI_GID=100

# if a prefix is used, also update urlprefix and internalurlprefix in clam_custom.config.yml
export URLPREFIX=tscan

# By default, data from the webservice will be stored on the mount you provide
# Make sure it exists!
export CLAM_ROOT=/data/www-data
export CLAM_PORT=80
# (set to true or false, enable this if you run behind a properly configured reverse proxy only)
export CLAM_USE_FORWARDED_HOST=false

# By default, there is no authentication on the service,
# which is most likely not what you want if you aim to
# deploy this in a production EXPORTironment.
# You can connect your own Oauth2/OpenID Connect authorization by setting the following EXPORTironment parameters:
export CLAM_OAUTH=false
#^-- set to true to enable
export CLAM_OAUTH_AUTH_URL=""
#^-- example for clariah: https://authentication.clariah.nl/Saml2/OIDC/authorization
export CLAM_OAUTH_TOKEN_URL=""
#^-- example for clariah https://authentication.clariah.nl/OIDC/token
export CLAM_OAUTH_USERINFO_URL=""
#^--- example for clariah: https://authentication.clariah.nl/OIDC/userinfo
export CLAM_OAUTH_CLIENT_ID=""
export CLAM_OAUTH_CLIENT_SECRET=""
#^-- always keep this private!

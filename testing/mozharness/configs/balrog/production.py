config = {
    'balrog_servers': [
        {
            'balrog_api_root': 'https://aus4-admin.mozilla.org/api',
            'ignore_failures': False,
            'url_replacements': [
                ('http://ftp.mozilla.org/pub/mozilla.org', 'http://download.cdn.mozilla.net/pub/mozilla.org'),
            ],
            'balrog_usernames': {
                'b2g': 'b2gbld',
                'firefox': 'ffxbld',
                'thunderbird': 'tbirdbld',
                'mobile': 'ffxbld',
                'Fennec': 'ffxbld',
                'graphene': 'ffxbld',
                'horizon': 'ffxbld',
            }
        },
        {
            'balrog_api_root': 'https://aus4-admin-dev.allizom.org/api',
            'ignore_failures': True,
            'balrog_usernames': {
                'b2g': 'stage-b2gbld',
                'firefox': 'stage-ffxbld',
                'thunderbird': 'stage-tbirdbld',
                'mobile': 'stage-ffxbld',
                'Fennec': 'stage-ffxbld',
                'graphene': 'stage-ffxbld',
                'horizon': 'stage-ffxbld',
            }
        }
    ]
}

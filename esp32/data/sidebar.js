(function() {
  // Sidebar toggle
  var sidebar = document.getElementById('sidebar');
  var menuToggle = document.getElementById('menu-toggle');
  var SIDEBAR_KEY = 'wan-watcher-sidebar';

  // Safe localStorage access
  function getStorage(key) {
    try {
      return localStorage.getItem(key);
    } catch (e) {
      return null;
    }
  }

  function setStorage(key, value) {
    try {
      localStorage.setItem(key, value);
    } catch (e) {
      // Ignore storage errors
    }
  }

  function updateMenuToggleTitle() {
    var isCollapsed = sidebar.classList.contains('collapsed');
    menuToggle.title = isCollapsed ? 'Open menu' : 'Close menu';
    menuToggle.setAttribute('aria-expanded', !isCollapsed);
  }

  // Expand sidebar only if user explicitly chose expanded
  if (getStorage(SIDEBAR_KEY) === 'expanded') {
    sidebar.classList.remove('collapsed');
  }
  updateMenuToggleTitle();

  menuToggle.addEventListener('click', function() {
    sidebar.classList.toggle('collapsed');
    var isCollapsed = sidebar.classList.contains('collapsed');
    setStorage(SIDEBAR_KEY, isCollapsed ? 'collapsed' : 'expanded');
    document.documentElement.classList.toggle('sidebar-expanded', !isCollapsed);
    updateMenuToggleTitle();
  });

  // Load version info
  fetch('/version.json')
    .then(function(r) {
      if (!r.ok) throw new Error('HTTP ' + r.status);
      return r.json();
    })
    .then(function(v) {
      var commitUrl = 'https://github.com/kevinabrandon/wan-watcher/commit/' + v.git_hash_full;
      var el = document.getElementById('version-info');
      if (el) {
        el.innerHTML = 'v' + v.version + ' (<a href="' + commitUrl + '" title="View commit on GitHub"><code>' + v.git_hash + '</code></a>) &bull; ' + v.build_time;
      }
    })
    .catch(function() {});
})();

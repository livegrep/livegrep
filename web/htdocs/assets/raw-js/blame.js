(function f() {
    var body = $('body');

    /* In the file view, highlight the contents of each diff whose
       commit the user mouses over. */

    if (body.hasClass('blamefile')) {
        body.on('mouseenter', '#hashes > a', function(e) {
            var href = $(e.target).attr('href') || "";
            var i = href.indexOf('.');
            if (i == -1) return;
            var commitHash = href.substring(0, i);
            var cls = 'highlight ' + href.substr(i + 1, 1);
            $('#hashes a[href^="' + commitHash + '"]').addClass(cls);
        });
        body.on('mouseleave', '#hashes > a', function(e) {
            var href = $(e.target).attr('href') || "";
            var i = href.indexOf('.');
            if (i == -1) return;
            var commitHash = href.substring(0, i);
            var cls = 'highlight ' + href.substr(i + 1, 1);
            $('#hashes a[href^="' + commitHash + '"]').removeClass(cls);
        });
    }

    /* When the user clicks a hash, remember the line's y coordinate,
       and warp it back to its current location when we land. */

    body.on('click', '#hashes > a', function(e) {
        var y = $(e.currentTarget).offset().top - body.scrollTop();
        Cookies.set("target_y", y, {expires: 1});
        // (Then, let the click proceed with its usual effect.)
    });

    var previous_y = Cookies.get("target_y");
    if (typeof previous_y !== "undefined") {
        Cookies.remove("target_y");
    }

    $(document).ready(function() {
        var target = $(":target");
        if (target.length === 0)
            return;
        var y = previous_y;
        if (typeof y === "undefined")
            y = $(window).height() / 2; // default to middle of viewport
        scroll_y = target.offset().top - y;
        scroll_y = Math.max(0, scroll_y); // clip to lower bound of 0
        window.scrollTo(0, scroll_y);
    });
})();
